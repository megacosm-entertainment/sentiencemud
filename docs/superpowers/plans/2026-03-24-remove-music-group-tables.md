# Remove music_table[] and group_table[] — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove legacy static `music_table[]` and `group_table[]` arrays, migrating all active references to the dynamic `SONG_DATA` and `SKILL_GROUP` APIs.

**Architecture:** The bootstrap path (JSON files exist in `data/skill_groups/` and `data/songs.json`) means the static tables are dead weight. We convert all runtime code to use `SKILL_GROUP`/`SONG_DATA` lookups via `skill_group_find()`, `song_lookup()`, and `known_groups` LLIST. The legacy `group_known[MAX_GROUP]` bool array is also removed (explicitly marked for Phase 9 removal). `gn_add`/`gn_remove`/`group_lookup` change signatures from int-index to `SKILL_GROUP*`-based.

**Tech Stack:** C, Jansson JSON, custom LLIST API

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `merc.h` | Modify | Remove `struct music_type`, `struct group_type`, externs, `MAX_GROUP`, `group_known[]`, update `gn_add`/`gn_remove`/`group_lookup` signatures |
| `const.c` | Modify | Remove `music_table[]` (~33 lines) and `group_table[MAX_GROUP]` (~245 lines) definitions |
| `skills.c` | Modify | Rewrite `group_lookup`, `gn_add`, `gn_remove`, `is_global_skill`, `has_subclass_skill`, `has_class_skill` |
| `save.c` | Modify | Migrate legacy .dat save, `fix_character`, `descrew_subclasses`, `find_class_skill` |
| `io/json/json_char.c` | Modify | Migrate JSON save and both JSON load paths for groups |
| `act_class.c` | Modify | Remove legacy `group_known` backward-compat block |
| `db2.c` | Modify | Rewrite `do_dump` group output |
| `script_mpcmds.c` | Modify | Update `gn_add`/`gn_remove` calls |
| `script_opcmds.c` | Modify | Update `gn_add`/`gn_remove` calls |
| `script_rpcmds.c` | Modify | Update `gn_add`/`gn_remove` calls |
| `script_tpcmds.c` | Modify | Update `gn_add`/`gn_remove` calls |
| `song_data.c` | Modify | Remove `bootstrap_songs()` music_table dependency |
| `skill_group.c` | Modify | Remove `bootstrap_groups_from_table()` group_table dependency |
| `mem.c` | Modify | Remove `group_known` zero-init if explicit (it's via struct zero, so no change needed) |
| `db.c` | Modify | Update boot comments |

---

## Task 1: Create helper function `char_knows_group()`

Before removing `group_known[]`, create a clean helper that checks `known_groups` LLIST.

**Files:**
- Modify: `src/skills.c` (add function)
- Modify: `src/merc.h` (add declaration)

- [ ] **Step 1: Add `char_knows_group()` to skills.c**

After the existing `group_lookup` function (~line 1432), add:

```c
/**
 * char_knows_group - Check if a character knows a skill group
 *
 * Searches the character's known_groups LLIST for the given group.
 *
 * @param ch    Character to check (must be PC with pcdata)
 * @param sg    SKILL_GROUP to look for
 * @return      true if the character knows this group
 */
bool char_knows_group(CHAR_DATA *ch, SKILL_GROUP *sg)
{
    if (!ch || !ch->pcdata || !sg)
        return false;

    return list_hasdata(ch->pcdata->known_groups, sg);
}
```

- [ ] **Step 2: Add declaration to merc.h**

Near the existing `group_lookup` declaration (~line 11105), add:

```c
bool    char_knows_group  args( ( CHAR_DATA *ch, SKILL_GROUP *sg ) );
```

- [ ] **Step 3: Build to verify**

```bash
cd /sentience/src && ./build tests 2>&1 | tail -5
```
Expected: Build succeeds (new function unused but compiled).

- [ ] **Step 4: Commit**

```bash
cd /sentience/src && git add skills.c merc.h && git commit -m "refactor(phase9): add char_knows_group() helper

Utility function for checking known_groups LLIST, replacing
group_known[] array checks as we remove group_table[].

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 2: Rewrite `group_lookup()`, `gn_add()`, `gn_remove()` in skills.c

These are the core functions that bridge group_table int indices to SKILL_GROUP pointers.

**Files:**
- Modify: `src/skills.c`
- Modify: `src/merc.h` (update declarations)

- [ ] **Step 1: Rewrite `group_lookup()` in skills.c**

The current function searches `group_table[]` by name and returns an int index. The new version returns a `SKILL_GROUP*` using `skill_group_search()` (which does prefix match, same as the old behavior).

Replace the existing `group_lookup` function (lines ~1418-1431) with:

```c
/**
 * group_lookup - Find a skill group by name prefix
 *
 * Searches loaded SKILL_GROUP entries for a prefix match.
 * Replaces the legacy group_table[] index lookup.
 *
 * @param name  Group name or prefix to search for
 * @return      Pointer to SKILL_GROUP, or NULL if not found
 */
SKILL_GROUP *group_lookup(const char *name)
{
    if (!name || !name[0])
        return NULL;

    return skill_group_search(name);
}
```

- [ ] **Step 2: Update `group_lookup` declaration in merc.h**

Change (~line 11105):
```c
// OLD:
int     group_lookup    args( (const char *name) );
// NEW:
SKILL_GROUP *group_lookup args( (const char *name) );
```

- [ ] **Step 3: Rewrite `gn_add()` in skills.c**

Replace the existing `gn_add` function (lines ~1435-1454) with:

```c
/**
 * gn_add - Grant a skill group to a character
 *
 * Adds the group to the character's known_groups list and grants
 * all skills contained in the group via group_add().
 *
 * @param ch    Character to grant the group to
 * @param sg    SKILL_GROUP to grant
 */
void gn_add(CHAR_DATA *ch, SKILL_GROUP *sg)
{
    ITERATOR it;
    char *skill_name;

    if (!ch || !ch->pcdata || !sg)
        return;

    if (!list_hasdata(ch->pcdata->known_groups, sg))
        list_appendlink(ch->pcdata->known_groups, sg);

    iterator_start(&it, sg->contents);
    while ((skill_name = (char *)iterator_nextdata(&it))) {
        group_add(ch, skill_name, false);
    }
    iterator_stop(&it);
}
```

- [ ] **Step 4: Update `gn_add` declaration in merc.h**

Change (~line 11106):
```c
// OLD:
void    gn_add          args( ( CHAR_DATA *ch, int gn) );
// NEW:
void    gn_add          args( ( CHAR_DATA *ch, SKILL_GROUP *sg) );
```

- [ ] **Step 5: Rewrite `gn_remove()` in skills.c**

Replace the existing `gn_remove` function (lines ~1458-1477) with:

```c
/**
 * gn_remove - Remove a skill group from a character
 *
 * Removes the group from the character's known_groups list and removes
 * all skills contained in the group via group_remove().
 *
 * @param ch    Character to remove the group from
 * @param sg    SKILL_GROUP to remove
 */
void gn_remove(CHAR_DATA *ch, SKILL_GROUP *sg)
{
    ITERATOR it;
    char *skill_name;

    if (!ch || !ch->pcdata || !sg)
        return;

    list_remlink(ch->pcdata->known_groups, sg, false);

    iterator_start(&it, sg->contents);
    while ((skill_name = (char *)iterator_nextdata(&it))) {
        group_remove(ch, skill_name);
    }
    iterator_stop(&it);
}
```

- [ ] **Step 6: Update `gn_remove` declaration in merc.h**

Change (~line 11107):
```c
// OLD:
void    gn_remove       args( ( CHAR_DATA *ch, int gn) );
// NEW:
void    gn_remove       args( ( CHAR_DATA *ch, SKILL_GROUP *sg) );
```

- [ ] **Step 7: Update `group_add()` in skills.c to use new `group_lookup()`**

The current `group_add` (lines ~1524-1532) uses `group_lookup` returning int:
```c
gn = group_lookup(name);
if (gn != -1)
{
    if (ch->pcdata->group_known[gn] == false)
        ch->pcdata->group_known[gn] = true;
    gn_add(ch,gn);
}
```

Replace with:
```c
    {
        SKILL_GROUP *sg = group_lookup(name);
        if (sg) {
            gn_add(ch, sg);
        }
    }
```

- [ ] **Step 8: Update `group_remove()` in skills.c to use new APIs**

The current `group_remove` (lines ~1553-1559) uses:
```c
gn = group_lookup(name);
if (gn != -1 && ch->pcdata->group_known[gn] == true)
{
    ch->pcdata->group_known[gn] = false;
    gn_remove(ch,gn);
}
```

Replace with:
```c
    {
        SKILL_GROUP *sg = group_lookup(name);
        if (sg && char_knows_group(ch, sg)) {
            gn_remove(ch, sg);
        }
    }
```

- [ ] **Step 9: Rewrite `is_global_skill()` in skills.c**

Replace (lines ~1993-2007):
```c
bool is_global_skill( int sn )
{
    if ( sn < 0 )
        return false;

    SKILL_GROUP *sg = skill_group_find("global skills");
    if (!sg)
        return false;

    ITERATOR it;
    char *skill_name;
    iterator_start(&it, sg->contents);
    while ((skill_name = (char *)iterator_nextdata(&it))) {
        if (!str_cmp(skill_name, skill_table[sn].name)) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}
```

- [ ] **Step 10: Rewrite `has_subclass_skill()` in skills.c**

Replace (lines ~2050-2090). The current code iterates `group_table` to find groups by name and check their spells. Use `sg->contents` instead:

```c
bool has_subclass_skill( int subclass, int sn )
{
    char *skill_name;

    if (sn < 0)
        return false;

    if (subclass < CLASS_WARRIOR_MARAUDER || subclass > CLASS_THIEF_SAGE)
        return false;

    skill_name = skill_table[sn].name;
    {
        CLASS_DATA *hss_class = class_from_legacy(0, subclass);
        if (hss_class && hss_class->groups) {
            ITERATOR git;
            SKILL_GROUP *sg;
            iterator_start(&git, hss_class->groups);
            while ((sg = (SKILL_GROUP *)iterator_nextdata(&git))) {
                ITERATOR sit;
                char *gskill;
                iterator_start(&sit, sg->contents);
                while ((gskill = (char *)iterator_nextdata(&sit))) {
                    if (!str_cmp(skill_name, gskill)) {
                        iterator_stop(&sit);
                        iterator_stop(&git);
                        return true;
                    }
                }
                iterator_stop(&sit);
            }
            iterator_stop(&git);
            return false;
        }
    }

    return false;
}
```

- [ ] **Step 11: Rewrite `has_class_skill()` in skills.c**

Replace (lines ~2093-2128):

```c
bool has_class_skill( int class, int sn )
{
    char *group_name;

    if (sn < 0)
        return false;

    if (class < CLASS_MAGE || class > CLASS_WARRIOR)
        return false;

    switch (class)
    {
    case CLASS_MAGE:     group_name = "mage skills"; break;
    case CLASS_CLERIC:   group_name = "cleric skills"; break;
    case CLASS_THIEF:    group_name = "thief skills"; break;
    case CLASS_WARRIOR:  group_name = "warrior skills"; break;
    default:             group_name = "global skills"; break;
    }

    SKILL_GROUP *sg = skill_group_find(group_name);
    if (!sg)
        return false;

    ITERATOR it;
    char *skill_name;
    iterator_start(&it, sg->contents);
    while ((skill_name = (char *)iterator_nextdata(&it))) {
        if (!str_cmp(skill_table[sn].name, skill_name)) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}
```

- [ ] **Step 12: Build (expect errors from callers not yet updated)**

```bash
cd /sentience/src && ./build tests 2>&1 | grep 'error:' | head -20
```

Note: Caller compilation errors are expected — fixed in subsequent tasks.

- [ ] **Step 13: Commit skills.c and merc.h changes**

```bash
cd /sentience/src && git add skills.c merc.h && git commit -m "refactor(phase9): rewrite group_lookup/gn_add/gn_remove to SKILL_GROUP API

group_lookup() now returns SKILL_GROUP* instead of int index.
gn_add()/gn_remove() take SKILL_GROUP* instead of int index.
is_global_skill(), has_subclass_skill(), has_class_skill() use
SKILL_GROUP contents iteration instead of group_table[].

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 3: Update all callers of group_lookup/gn_add/gn_remove

**Files:**
- Modify: `src/save.c`
- Modify: `src/io/json/json_char.c`
- Modify: `src/act_class.c`
- Modify: `src/db2.c`
- Modify: `src/script_mpcmds.c`
- Modify: `src/script_opcmds.c`
- Modify: `src/script_rpcmds.c`
- Modify: `src/script_tpcmds.c`

- [ ] **Step 1: Update save.c — legacy .dat save (lines ~994-1000)**

Replace:
```c
for (gn = 0; gn < MAX_GROUP; gn++)
    {
        if (group_table[gn].name != NULL && ch->pcdata->group_known[gn])
        {
            fprintf(fp, "Gr '%s'\n",group_table[gn].name);
        }
    }
```

With:
```c
    {
        ITERATOR sg_it;
        SKILL_GROUP *sg;
        iterator_start(&sg_it, ch->pcdata->known_groups);
        while ((sg = (SKILL_GROUP *)iterator_nextdata(&sg_it))) {
            if (sg->name)
                fprintf(fp, "Gr '%s'\n", sg->name);
        }
        iterator_stop(&sg_it);
    }
```

- [ ] **Step 2: Update save.c — `fix_character()` gn_add loop (lines ~3270-3273)**

Replace:
```c
    // Add groups it should know
    for(i=0;i < MAX_GROUP; i++)
        if( ch->pcdata->group_known[i] )
            gn_add(ch, i);
```

With:
```c
    // Add groups it should know
    {
        ITERATOR sg_it;
        SKILL_GROUP *sg;
        iterator_start(&sg_it, ch->pcdata->known_groups);
        while ((sg = (SKILL_GROUP *)iterator_nextdata(&sg_it))) {
            gn_add(ch, sg);
        }
        iterator_stop(&sg_it);
    }
```

- [ ] **Step 3: Update save.c — `fix_character()` migration block (lines ~3666-3677)**

Remove the entire migration block (it migrated `group_known[]` → `known_groups`; no longer needed since `group_known[]` is being removed):

```c
        /* Bootstrap known_groups LLIST from legacy group_known[] array.
         * The JSON load path now populates both, but characters loaded from
         * dat or from older JSON files may only have the bool array set. */
        if (ch->pcdata && list_size(ch->pcdata->known_groups) == 0) {
            for (int gn = 0; gn < MAX_GROUP; gn++) {
                if (ch->pcdata->group_known[gn] && group_table[gn].name) {
                    SKILL_GROUP *sg = skill_group_find(group_table[gn].name);
                    if (sg && !list_hasdata(ch->pcdata->known_groups, sg))
                        list_appendlink(ch->pcdata->known_groups, sg);
                }
            }
        }
```

Delete this entire block.

- [ ] **Step 4: Update save.c — `descrew_subclasses()` (lines ~3747-3803)**

Replace every `ch->pcdata->group_known[group_lookup("X skills")]` pattern with `char_knows_group(ch, skill_group_find("X skills"))`. For example:

```c
    // OLD:
    if (ch->pcdata->group_known[group_lookup("necromancer skills")] == true)
    // NEW:
    if (char_knows_group(ch, skill_group_find("necromancer skills")))
```

Apply this transformation to all 12 occurrences in this function.

- [ ] **Step 5: Update save.c — `find_class_skill()` (lines ~3895-3917)**

Replace:
```c
bool find_class_skill(CHAR_DATA *ch, int class)
{
    int gn;
    int i;

    switch (class)
    {
    case CLASS_MAGE:    gn = group_lookup("mage skills");    break;
    case CLASS_CLERIC:  gn = group_lookup("cleric skills");  break;
    case CLASS_THIEF:   gn = group_lookup("thief skills");   break;
    case CLASS_WARRIOR: gn = group_lookup("warrior skills");  break;
    default:
        pbugf(LOG_ERROR, "find_class_skill: bad class.");
        return false;
    }

    for (i = 0; group_table[gn].spells[i] != NULL; i++) {
    if (get_skill(ch, skill_lookup(group_table[gn].spells[i])) > 0)
        return true;
    }

    return false;
}
```

With:
```c
bool find_class_skill(CHAR_DATA *ch, int class)
{
    char *group_name;

    switch (class)
    {
    case CLASS_MAGE:    group_name = "mage skills";    break;
    case CLASS_CLERIC:  group_name = "cleric skills";  break;
    case CLASS_THIEF:   group_name = "thief skills";   break;
    case CLASS_WARRIOR: group_name = "warrior skills";  break;
    default:
        pbugf(LOG_ERROR, "find_class_skill: bad class.");
        return false;
    }

    SKILL_GROUP *sg = skill_group_find(group_name);
    if (!sg)
        return false;

    ITERATOR it;
    char *skill_name;
    iterator_start(&it, sg->contents);
    while ((skill_name = (char *)iterator_nextdata(&it))) {
        if (get_skill(ch, skill_lookup(skill_name)) > 0) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}
```

- [ ] **Step 6: Update io/json/json_char.c — JSON save (lines ~980-1003)**

Replace the entire else-branch fallback (which used `group_known[]` + `group_table[]`):

```c
    } else {
        for (gn = 0; gn < MAX_GROUP; gn++) {
            if (ch->pcdata->group_known[gn] && group_table[gn].name) {
                json_t *group_data = json_object();
                json_object_set_new(group_data, "id", json_integer(gn));
                json_object_set_new(group_data, "name", json_string(group_table[gn].name));
                json_array_append_new(groups, group_data);
            }
        }
    }
```

With just the closing brace (remove the entire fallback — `known_groups` is always populated now):
```c
    }
```

Also in the primary branch, remove the `group_lookup` call for the "id" field. Replace:
```c
            int gn = group_lookup(sg->name);
            json_object_set_new(group_data, "id", json_integer(gn >= 0 ? gn : -1));
```
With:
```c
            json_object_set_new(group_data, "id", json_integer(-1));
```

- [ ] **Step 7: Update io/json/json_char.c — both JSON load paths (lines ~4688-4711 and ~5234-5258)**

Replace both identical blocks. The current code:
```c
    json_array_foreach(skill_groups, index, array_elem) {
        const char *gname = json_get_string(array_elem, "name", "");
        int gn = -1;
        if (gname && gname[0])
            gn = group_lookup(gname);
        if (gn < 0)
            gn = json_integer_value(json_object_get(array_elem, "id"));
        if (gn >= 0 && gn < MAX_GROUP) {
            ch->pcdata->group_known[gn] = true;
            if (group_table[gn].name) {
                SKILL_GROUP *sg = skill_group_find(group_table[gn].name);
                if (sg && !list_hasdata(ch->pcdata->known_groups, sg))
                    list_appendlink(ch->pcdata->known_groups, sg);
            }
        }
    }
```

Replace with:
```c
    json_array_foreach(skill_groups, index, array_elem) {
        const char *gname = json_get_string(array_elem, "name", "");
        if (gname && gname[0]) {
            SKILL_GROUP *sg = skill_group_find(gname);
            if (sg && !list_hasdata(ch->pcdata->known_groups, sg))
                list_appendlink(ch->pcdata->known_groups, sg);
        }
    }
```

- [ ] **Step 8: Update act_class.c — remove legacy group_known backward compat (lines ~336-345)**

Remove the entire block:
```c
    /* Also set legacy group_known flag for backward compat */
    {
        int g;
        for (g = 0; g < MAX_GROUP; g++) {
            if (group_table[g].name && !str_cmp(group_table[g].name, name)) {
                ch->pcdata->group_known[g] = true;
                break;
            }
        }
    }
```

- [ ] **Step 9: Update db2.c — rewrite `do_dump` group output (lines ~1205-1219)**

Replace:
```c
    i = 0;
    while ( group_table[i].name != NULL )
    {
        fprintf( fp, "[%s]:\n", group_table[i].name );
        n = 0;
        while ( group_table[i].spells[n] != NULL )
        {
        fprintf( fp, "%i) %s\n", n+1, group_table[i].spells[n] );
        n++;
        }

        fprintf( fp, "\n");

        i++;
    }
```

With:
```c
    {
        SKILL_GROUP *sg;
        for (sg = skill_group_first(); sg; sg = sg->next) {
            ITERATOR it;
            char *skill_name;
            int n = 0;
            fprintf(fp, "[%s]:\n", sg->name);
            iterator_start(&it, sg->contents);
            while ((skill_name = (char *)iterator_nextdata(&it))) {
                fprintf(fp, "%i) %s\n", ++n, skill_name);
            }
            iterator_stop(&it);
            fprintf(fp, "\n");
        }
    }
```

- [ ] **Step 10: Update all 4 script_*pcmds.c files**

Each file has an identical pattern. Replace (example from script_mpcmds.c ~4706-4721):
```c
    gn = group_lookup(arg->d.str);
    if( gn != -1)
    {
        if( fAdd )
        {
            if( !mob->pcdata->group_known[gn] )
                gn_add(mob,gn);
        }
        else
        {
            if( mob->pcdata->group_known[gn] )
                gn_remove(mob,gn);
        }
    }
```

With:
```c
    {
        SKILL_GROUP *sg = group_lookup(arg->d.str);
        if (sg) {
            if (fAdd) {
                if (!char_knows_group(mob, sg))
                    gn_add(mob, sg);
            } else {
                if (char_knows_group(mob, sg))
                    gn_remove(mob, sg);
            }
        }
    }
```

Apply to: `script_mpcmds.c`, `script_opcmds.c`, `script_rpcmds.c`, `script_tpcmds.c`.

- [ ] **Step 11: Build to verify all callers compile**

```bash
cd /sentience/src && ./build tests 2>&1 | tail -10
```

Expected: Clean build.

- [ ] **Step 12: Commit**

```bash
cd /sentience/src && git add save.c io/json/json_char.c act_class.c db2.c script_mpcmds.c script_opcmds.c script_rpcmds.c script_tpcmds.c && git commit -m "refactor(phase9): migrate all group_table/group_known callers to SKILL_GROUP API

Update save.c, json_char.c, act_class.c, db2.c, and script_*pcmds.c
to use SKILL_GROUP pointers and known_groups LLIST instead of
group_table[] indices and group_known[] bool array.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 4: Remove table definitions, structs, and bootstrap code

**Files:**
- Modify: `src/const.c`
- Modify: `src/merc.h`
- Modify: `src/song_data.c`
- Modify: `src/skill_group.c`
- Modify: `src/db.c`

- [ ] **Step 1: Remove `music_table[]` from const.c (lines 852-885)**

Delete the entire block from `const struct music_type music_table[MAX_SONGS] =` through the closing `};`.

- [ ] **Step 2: Remove `group_table[MAX_GROUP]` from const.c (lines 2857-3102)**

Delete the entire block from `const struct group_type group_table [MAX_GROUP] =` through the closing `};`. (Note: line numbers will have shifted from step 1.)

- [ ] **Step 3: Remove struct music_type from merc.h (lines ~8603-8613)**

Delete:
```c
struct music_type
{
    char *  name;
    int     level;
    char *  spell1;
    char *  spell2;
    char *  spell3;
    int16_t beats;
    int16_t mana;
    int16_t target;
};
```

- [ ] **Step 4: Remove struct group_type from merc.h (lines ~8641-8646)**

Delete:
```c
struct  group_type
{
    char *  name;
    int16_t rating[MAX_CLASS];
    char *  spells[MAX_IN_GROUP];
};
```

- [ ] **Step 5: Remove extern declarations from merc.h**

Delete (~line 9531):
```c
extern  const   struct  music_type  music_table [];
```

Delete (~line 9542):
```c
extern  const   struct  group_type  group_table [MAX_GROUP];
```

- [ ] **Step 6: Remove `group_known[MAX_GROUP]` from pcdata in merc.h**

Delete (~line 5894):
```c
    bool        group_known [MAX_GROUP];  /* Legacy — kept during migration (Phase 9 removal) */
```

- [ ] **Step 7: Remove `MAX_GROUP` from merc.h if no other users**

Verify no remaining references:
```bash
cd /sentience/src && grep -rn '\bMAX_GROUP\b' --include='*.c' --include='*.h' . | grep -v 'tests/'
```
If none remain, delete (~line 1095):
```c
#define MAX_GROUP       30
```

- [ ] **Step 8: Remove commented-out music_table code in save.c**

Delete the commented-out block at ~lines 973-992:
```c
    /*
    // Save song list
    for (sn = 0; sn < MAX_SONGS && music_table[sn].name; sn++)
        ...
    */
```

And at ~lines 4051-4069:
```c
/*
    for (sn = 0; sn < MAX_SONGS && music_table[sn].name; sn++)
        ...
*/
```

- [ ] **Step 9: Remove `bootstrap_songs()` music_table dependency in song_data.c**

Replace the `bootstrap_songs()` function body (lines ~184-215) with a stub that logs an error:
```c
bool bootstrap_songs(void)
{
    log_string("bootstrap_songs: ERROR — music_table[] removed in Phase 9. "
               "Song JSON files must exist in data/songs.json.");
    return false;
}
```

Update the comment block above it. Also update the log message at line ~298.

- [ ] **Step 10: Remove `bootstrap_groups_from_table()` group_table dependency in skill_group.c**

Replace the `bootstrap_groups_from_table()` function body (lines ~346-384) with a stub:
```c
static void bootstrap_groups_from_table(void)
{
    log_string("bootstrap_groups_from_table: ERROR — group_table[] removed in Phase 9. "
               "Skill group JSON files must exist in data/skill_groups/.");
}
```

Update comments in `load_skill_groups()` that mention group_table[].

- [ ] **Step 11: Update db.c boot comments (lines ~1307, 1311)**

Change:
```c
    // On first run, bootstraps from legacy group_table[] and saves JSON files.
```
To:
```c
    // Loads skill groups from data/skill_groups/ JSON files.
```

And:
```c
    // On first run, bootstraps from legacy music_table[] and saves JSON files.
```
To:
```c
    // Loads songs from data/songs.json.
```

- [ ] **Step 12: Build and test**

```bash
cd /sentience/src && ./build tests 2>&1 | tail -10
```

Expected: Clean build.

```bash
cd /sentience && ./sent -test 2>&1 | grep -c 'PASS'
```

Expected: Same pass count as baseline (64+).

- [ ] **Step 13: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "refactor(phase9): remove music_table[], group_table[], and group_known[]

Remove legacy static arrays and structures superseded by SONG_DATA
and SKILL_GROUP dynamic backends. Remove group_known[] bool array
from pcdata (marked for Phase 9 removal). Convert bootstrap functions
to stubs that log errors if JSON files are missing.

Removed from merc.h: struct music_type, struct group_type,
group_known[MAX_GROUP], MAX_GROUP, extern declarations.
Removed from const.c: ~280 lines of static table data.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 5: Final verification and squash commit

- [ ] **Step 1: Clean build from scratch**

```bash
cd /sentience/src && ./build clean tests 2>&1 | tail -10
```

- [ ] **Step 2: Run tests**

```bash
cd /sentience && ./sent -test 2>&1 | grep -c 'PASS'
```

Expected: ≥64 passes (same as baseline).

- [ ] **Step 3: Verify no remaining references**

```bash
cd /sentience/src && grep -rn '\bmusic_table\b\|\bgroup_table\b\|\bgroup_known\b\|\bMAX_GROUP\b\|\bmusic_type\b\|\bgroup_type\b' --include='*.c' --include='*.h' . | grep -v 'tests/' | grep -v '\.build/'
```

Expected: Only comments/docs references remain (song_data.h header comment, skill_group.h header comment, etc). No active code references.

- [ ] **Step 4: Squash into single commit if preferred**

```bash
git rebase -i HEAD~4
```

Or keep as separate logical commits.

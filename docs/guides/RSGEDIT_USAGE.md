# RSGEdit Usage and Integration Guide

This document covers:

1. How to build and maintain random string generators with `rsgedit`
2. Practical patterns for masculine/feminine naming
3. How to integrate RSG generation into other editors and scripting

---

## 1) What `rsgedit` Is For

`rsgedit` manages named random-string generators (RSGs).

Each generator contains:
- **Patterns** (templates with weighted selection)
- **Classes** (token groups with weighted entries)

At generation time:
- a pattern is chosen by weight
- each `{class_name}` token in the pattern is replaced with a weighted random entry from that class
- `{{` inside templates emits a literal `{`

---

## 2) Command Reference

## Entering the editor

- `rsgedit list`
- `rsgedit create <name>`
- `rsgedit <uid|name>`
- `rsgedit reload` (top-level cache reload)

Once inside a generator:
- `show`
- `name <new name>`
- `description <text>`
- `generate [count]`
- `pattern ...` (subcommands)
- `class ...` (subcommands)
- `reload` (reload and keep same generator open if still present)
- `save`

## Pattern subcommands

- `pattern list`
- `pattern create <weight> <template>`
- `pattern show <index>`
- `pattern delete <index>`
- `pattern ?`

## Class subcommands

- `class list`
- `class create <name>`
- `class show <uid|name>`
- `class delete <uid|name>`
- `class add <uid|name> <weight> <text>`
- `class edit <uid|name> <index> <weight> <text>`
- `class remove <uid|name> <index>`
- `class ?`

---

## 3) Step-by-Step: Build a Generator From Scratch

Example target: fantasy character names.

### Step 1: Create and open the generator

- `rsgedit create fantasy_names`

### Step 2: Add description

- `description Base generator for fantasy NPC names`

### Step 3: Create classes

- `class create prefix`
- `class create root`
- `class create suffix`

### Step 4: Fill classes with weighted entries

- `class add prefix 30 Al`
- `class add prefix 30 Bel`
- `class add prefix 10 Kae`

- `class add root 50 dar`
- `class add root 50 ryn`
- `class add root 20 thal`

- `class add suffix 40 ion`
- `class add suffix 40 ia`
- `class add suffix 20 os`

### Step 5: Add weighted patterns

- `pattern create 80 {prefix}{root}{suffix}`
- `pattern create 20 {prefix}{root}`

### Step 6: Validate output

- `generate 10`
- `pattern list`
- `class show prefix`

### Step 7: Save and verify persistence

- `save`
- `reload`
- `show`

### Step 8: External JSON edits workflow

If editing generator files directly under `data/system/rsg_generators/*.json`:

1. Edit file(s)
2. In game, run `rsgedit reload` (top-level or in-editor)
3. Re-open generator and verify with `show` / `generate`

Notes:
- Loader accepts non-hidden `.json` files in `rsg_generators`
- Duplicate UIDs/names will be ignored at load time to avoid collisions

---

## 4) Masculine/Feminine Name Strategies

There are two recommended approaches.

## Approach A (recommended now): explicit gendered classes/patterns

Use separate classes and patterns for clarity and control.

Example classes:
- `first_masc`
- `first_fem`
- `last_common`

Example setup:
- `class create first_masc`
- `class create first_fem`
- `class create last_common`

- `class add first_masc 50 Daren`
- `class add first_masc 50 Kalen`

- `class add first_fem 50 Lyra`
- `class add first_fem 50 Selene`

- `class add last_common 50 Ashford`
- `class add last_common 50 Vale`

Patterns:
- `pattern create 50 {first_masc} {last_common}`
- `pattern create 50 {first_fem} {last_common}`

This gives weighted control over masculine vs feminine outputs.

## Approach B: separate generators by use-case

Create independent generators, for example:
- `npc_names_masc`
- `npc_names_fem`
- `npc_names_neutral`

Use this when calling code already knows the required category and should not rely on mixed weighted patterns.

---

## 5) Template and Escaping Rules

- Placeholders use `{class_name}`
- Valid class token chars: letters, digits, `_`, `-`
- `{{` in a template means literal `{`
- Invalid `{...}` forms are treated as normal text (not placeholders)

Example:
- Template: `The {{Sigil}} of {house}`
- Possible output: `The {Sigil} of Vaelor`

---

## 6) Integration Plan for Other Editors and Scripting

Goal: use RSG wherever a dynamic string is useful (NPC names, object names/descriptions, script-generated text).

We need two generation modes:

1. **Any template mode**
   - Use generator’s weighted patterns as-is
   - Equivalent to current `generate`

2. **Specified template/combination mode**
   - Caller can provide a template override, or select a specific pre-defined pattern/combo

## Proposed contract (engine-level)

Add a shared API surface (names are examples):

- `bool rsg_generate_any(long rsg_uid, char *out, size_t out_size);`
- `bool rsg_generate_template(long rsg_uid, const char *template, char *out, size_t out_size);`
- `bool rsg_generate_pattern(long rsg_uid, int pattern_index, char *out, size_t out_size);`

Where:
- `rsg_generate_any` picks weighted pattern from generator
- `rsg_generate_template` uses caller-provided template (`{class}` substitutions)
- `rsg_generate_pattern` forces a known stored pattern

## Editor integration pattern

For editors that accept free text fields (`medit`, `oedit`, `redit`, etc.), support one of:

- **Literal mode**: existing behavior
- **RSG any mode**: value generated from generator UID/name
- **RSG template mode**: generated from `generator + template override`
- **RSG pattern mode**: generated from `generator + pattern index`

Suggested UX shape:
- field-level toggle (literal vs RSG)
- if RSG enabled, fields for generator and mode
- optional preview command to show sample outputs

## Scripting integration pattern

Expose script-level helpers such as:

- `rsggenerate(<generator>)`
- `rsggenerate(<generator>, <template>)`
- `rsggeneratepattern(<generator>, <index>)`

Use cases:
- NPC spawn naming
- dynamic loot/item naming
- room flavor text and event text
- mission/quest text variants

---

## 7) Practical Design Rules for Reuse

When building generators intended for broad reuse:

1. Prefer stable class names (`first_masc`, `first_fem`, `surname`, `title`, `material`, `quality`)
2. Keep templates readable and composable
3. Separate domain-specific classes (e.g., demon-only suffixes) into dedicated generators
4. Use weights for rarity instead of duplicated entries
5. Document intended consumers in `description`

---

## 8) Operational Checklist (Staff)

When shipping a new generator:

1. Create generator and description
2. Add classes and entries
3. Add patterns and verify refs (`pattern show`)
4. Run `generate 20` and inspect quality
5. Save
6. Reload and re-verify
7. If externally edited, reload and re-verify again
8. Record intended consumers (which editors/scripts should use it)

---

## 9) Current Limits and Follow-up Work

Current behavior:
- weighted generation works
- placeholder parsing is strict and safe with `{{` escape
- reload supports external file edits

Still needed for full cross-editor adoption:
- shared generation API callable from all subsystems (not editor-local only)
- editor field schema support for RSG modes
- scripting function wrappers for RSG generation
- optional deterministic seed support for reproducibility (future)

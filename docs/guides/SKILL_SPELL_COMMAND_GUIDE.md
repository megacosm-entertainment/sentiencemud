# Adding a New Skill, Spell, or Command

This guide covers the required code and data steps for adding gameplay actions in the current Sentience architecture.

Scope:
- Skill (non-spell ability)
- Spell (skill with a `spell_fun` implementation)
- Command (player/admin command routed by `commands.json`)

---

## 1) Add a New Command

Commands are runtime-configured through `data/system/commands.json` (edited in-game via `cmdedit`), but command functions are still C functions that must be registered.

### Required code steps

1. Implement `do_yourcommand(CHAR_DATA *ch, char *argument)` in the appropriate `act_*.c` (or another existing module).
2. Add `DECLARE_DO_FUN(do_yourcommand);` in `interp.h`.
3. Add function mapping in `tables.c` (`do_func_table`):
   - `{ "do_yourcommand", do_yourcommand },`
4. If you added a new `.c` file, update both build systems:
   - `CMakeLists.txt`
   - `Makefile`

### Required runtime/data steps

1. In-game: `cmdedit create yourcommand`
2. Set at minimum:
   - `function do_yourcommand`
   - `position ...`
   - `rank ...`
   - `type ...`
   - `enabled true`
3. Add summary/help metadata (`summary`, `sethelp`) so it shows correctly in help/cmdlist.
4. Save command definitions (`cmdedit`/save flow writes `commands.json`).

### Commit-time bootstrap note

Only `src/` is version-controlled. If this command must exist in fresh environments, also update:
- `src/bootstrap/bootstrap_data/system/commands.json`

---

## 2) Add a New Skill (Non-Spell)

Skills are data-driven (`skill_data`), persisted as JSON under `data/skills/`.

### Required data steps

1. Create/update skill JSON entry (via tooling/workflow used by your environment).
2. Ensure key fields are present and valid:
   - `name`, `display`, `summary`
   - `target`, `position`, `beats`, `min_mana`
   - `flags`, `difficulty`
   - `spell_fun_name` should be `spell_null` for non-spell skills
3. Ensure it is reachable by gameplay:
   - Add to one or more skill groups (`data/skill_groups/*.json`), and/or
   - Add via class rewards (`data/classes/*.json`).

### Optional code steps

- If the skill behavior needs custom C logic beyond existing systems, implement that logic in the relevant gameplay file.
- If you add new source files, update both `CMakeLists.txt` and `Makefile`.

### Bootstrap note

For first-run seed availability in clean environments, mirror data changes into:
- `src/bootstrap/bootstrap_data/skills/`
- `src/bootstrap/bootstrap_data/skill_groups/`
- `src/bootstrap/bootstrap_data/classes/` (if class rewards changed)

---

## 3) Add a New Spell

A spell is a skill that points at a C spell function.

### Required code steps

1. Implement `spell_yourspell(...)` in an appropriate `magic_*.c` file.
2. Add `DECLARE_SPELL_FUN(spell_yourspell);` in `merc.h`.
3. Register the function name/pointer in `skill_data.c` (`spell_func_table`):
   - `{ "spell_yourspell", spell_yourspell },`
4. If you created a new source file, update both build systems:
   - `CMakeLists.txt`
   - `Makefile`

### Required data steps

1. Create/update the spell skill JSON in `data/skills/`.
2. Set `spell_fun_name` to `spell_yourspell`.
3. Set spell metadata (`target`, `min_mana`, `beats`, flags, etc.).
4. Add the spell to skill groups and/or class rewards so players can acquire it.

### Bootstrap note

If the spell must exist after `-bootstrap` in a clean environment, also update bootstrap seed JSON under `src/bootstrap/bootstrap_data/`.

---

## 4) Validation Checklist (All Three)

1. Build:
   - `cd /sentience/src && ./build`
2. Run and smoke-test in game:
   - command appears in `cmdlist` and executes
   - skill/spell appears where expected (groups/classes)
   - cast/use path works and logs/errors are clean
3. If you changed startup/default data, confirm bootstrap path still works in a clean data environment.

---

## 5) Common Failure Modes

- Command exists in JSON but does nothing:
  - Missing `do_func_table` entry in `tables.c`, or wrong `function` name in `commands.json`.
- Spell loads but cannot resolve function:
  - Missing `spell_func_table` entry in `skill_data.c`, or missing `DECLARE_SPELL_FUN` in `merc.h`.
- Feature works locally but not in fresh environments:
  - Bootstrap seed JSON under `src/bootstrap/bootstrap_data/` was not updated.
- Build passes in one system only:
  - New source file added to only one of `CMakeLists.txt` / `Makefile`.
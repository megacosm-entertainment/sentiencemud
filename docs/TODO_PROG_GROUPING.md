# TODO: Script/Prog Grouping on Entities

See [PLAN_PROG_GROUPING.md](PLAN_PROG_GROUPING.md) for full design.

## Phase 1: Grouped OLC Display

- [ ] Add `PROG_GROUP` / `PROG_GROUP_ENTRY` types to `editors/common.h`
- [ ] Implement `prog_build_groups()` in `editors/common.c`
- [ ] Implement `olc_show_progs_grouped()` in `editors/common.c`
- [ ] Switch medit_show to `olc_show_progs_grouped()` (medit.c:648)
- [ ] Switch oedit_show to `olc_show_progs_grouped()` (oedit.c:322)
- [ ] Switch redit_show to `olc_show_progs_grouped()` (redit.c:279)
- [ ] Switch tedit_show to `olc_show_progs_grouped()` (tedit.c:175)
- [ ] Switch aedit_show to `olc_show_progs_grouped()` (aedit.c:189)
- [ ] Replace inline display in bpedit_show (bpedit.c:287-314)
- [ ] Replace inline display in dngedit_show (dngedit.c:572-600)
- [ ] Build and test

## Phase 2: Updated OLC Commands

### Shared helpers (olc_act.c / olc.h)
- [ ] `edit_script_attached()` - check if script vnum exists on entity
- [ ] `edit_trigger_exists()` - check for exact (vnum, trig_type, phrase) duplicate
- [ ] `edit_delscript()` - remove all triggers for Nth script group (1-based)
- [ ] `edit_deltrigger_specific()` - remove specific trigger by type+phrase

### Update add*prog (auto-group + duplicate detection)
- [ ] medit_addmprog (medit.c)
- [ ] oedit_addoprog (oedit.c)
- [ ] redit_addrprog (redit.c)
- [ ] tedit_addtprog (tedit.c)
- [ ] aedit_addaprog (aedit.c)
- [ ] bpedit_addiprog (bpedit.c)
- [ ] dngedit_adddprog (dngedit.c)

### Update del*prog (1-based group deletion)
- [ ] medit_delmprog (medit.c)
- [ ] oedit_deloprog (oedit.c)
- [ ] redit_delrprog (redit.c)
- [ ] tedit_deltprog (tedit.c)
- [ ] aedit_delaprog (aedit.c)
- [ ] bpedit_deliprog (bpedit.c)
- [ ] dngedit_deldprog (dngedit.c)

### Add deltrigger command
- [ ] medit_deltrigger + register in medit_table (olc.c)
- [ ] oedit_deltrigger + register in oedit_table (olc.c)
- [ ] redit_deltrigger + register in redit_table (olc.c)
- [ ] tedit_deltrigger + register in tedit_table (olc.c)
- [ ] aedit_deltrigger + register in aedit_table (olc.c)
- [ ] bpedit_deltrigger + register in bpedit_table (blueprint.c)
- [ ] dngedit_deltrigger + register in dngedit_table (dungeon.c)
- [ ] Build and test

## Phase 3: JSON Format Change

- [ ] Rewrite `json_area_serialize_progs()` to use `prog_build_groups()` (json_area.c:3712)
- [ ] Extract old deserializer into `json_area_deserialize_progs_flat()` (json_area.c:3765)
- [ ] Implement `json_area_deserialize_progs_grouped()` for new format
- [ ] Update `json_area_deserialize_progs()` to detect format and dispatch
- [ ] Test: load old-format area, verify correct behavior
- [ ] Test: save area, verify new JSON format
- [ ] Test: reload saved area, verify round-trip integrity
- [ ] Build and test

## Phase 4: Reverse Lookup (`uses` command)

- [ ] Implement `edit_show_script_uses()` in olc_act.c
- [ ] Add `uses` to mpedit command table + handler
- [ ] Add `uses` to opedit command table + handler
- [ ] Add `uses` to rpedit command table + handler
- [ ] Add `uses` to tpedit command table + handler
- [ ] Add `uses` to apedit command table + handler
- [ ] Add `uses` to ipedit command table + handler
- [ ] Add `uses` to dpedit command table + handler
- [ ] Build and test

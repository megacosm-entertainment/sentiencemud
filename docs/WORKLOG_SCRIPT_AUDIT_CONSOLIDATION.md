# Worklog: Script Audit Consolidation (Phase 2)

**Date:** 2026-02-20

## Objective

Advance Phase 2 command consolidation, remove confirmed dead Lua scripting remnants, and prepare for next consolidation tranche with clear sequencing.

## Work Performed

1. **Completed Phase 2 Tranche A dispatch consolidation**
   - Consolidated var-family command routing in command tables for:
     - mob progs (`script_mpcmds.c`)
     - object progs (`script_opcmds.c`)
     - room progs (`script_rpcmds.c`)
     - token progs (`script_tpcmds.c`)
   - Routed these commands to shared handlers in `script_commands.c`:
     - `scriptcmd_varset`
     - `scriptcmd_varseton`
     - `scriptcmd_varclear`
     - `scriptcmd_varclearon`
     - `scriptcmd_varcopy`
     - `scriptcmd_varsave`
     - `scriptcmd_varsaveon`

2. **Verified and removed dead Lua script module artifacts**
   - Confirmed `script_lua.c` was not included in active build source lists (`CMakeLists.txt`, `Makefile`).
   - Confirmed no active `src/` references to Lua execution lifecycle functions (`script_loadlua`, `script_readlua`, `script_freelua`, `execute_lua_script`).
   - Removed `/sentience/src/script_lua.c`.
   - Removed obsolete `SCRIPT_LUA` flag define from `/sentience/src/scripts.h`.

3. **Preserved consolidation sequencing discipline**
   - Maintained prior plan direction: complete consolidation before additional non-critical micro-hardening to avoid duplicate edits.

## Validation

- Debug build completed successfully.
- Focused script regression suite passed:
  - `./sent -test:script_engine` => 11/11 PASS

## Next Steps

1. Execute Phase 2 Tranche B consolidation for room-resolution-heavy command families:
   - `transfer`
   - `goto`
   - `force`
   - `echo` variants
2. Keep migration incremental (dispatch/routing first, body elimination after parity is validated).
3. Re-run targeted script tests after each family migration.

## Follow-up Update (2026-02-20, same session)

1. **Started Tranche B with shared `goto` handler**
   - Added `scriptcmd_goto` in `script_commands.c`.
   - Preserved existing behavior branches for mob/object/token contexts.
   - Routed command-table `goto` dispatch to shared handler in:
     - `script_mpcmds.c`
     - `script_opcmds.c`
     - `script_tpcmds.c`

2. **Validation after Tranche B kickoff change**
   - Build completed successfully.
   - `./sent -test:script_engine` remained green (11/11 pass).

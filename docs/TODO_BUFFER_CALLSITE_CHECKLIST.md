# TODO: BUFFER Callsite Checklist (Post-Editor Sweep)

Generated: 2026-02-27
Method: heuristic scan (`new_buf` + line-level `add_buf` / `bprintf` checks)
Note: Counts are approximate and include false positives where checks happen in helper wrappers.

## Core priority (next)
- `act_wiz.c` — add_buf unchecked: 190, bprintf unchecked: 24
- `script_expand.c` — add_buf unchecked: 153
- `church.c` — add_buf unchecked: 67
- `magic_law.c` — add_buf unchecked: 64
- `olc_act.c` — add_buf unchecked: 52
- `blueprint.c` — add_buf unchecked: 10, bprintf unchecked: 47
- `dungeon.c` — add_buf unchecked: 2, bprintf unchecked: 48
- `project.c` — add_buf unchecked: 34
- `act_info.c` — add_buf unchecked: 32
- `skills.c` — add_buf unchecked: 30
- `act_info2.c` — add_buf unchecked: 23
- `act_class.c` — add_buf unchecked: 22
- `help.c` — add_buf unchecked: 16

## Core medium
- `wilds.c` — 19
- `stats.c` — 16
- `mail.c` — 15
- `olc.c` — 12
- `script_commands.c` — 11
- `script_vars.c` — 9
- `chat_rooms.c` — 7
- `scripts.c` — 7
- `channels/channel_moderation.c` — 6
- `script_mpcmds.c` — 6
- `script_rpcmds.c` — 6
- `treasuremap.c` — 6
- `script_opcmds.c` — 5

## Core low
- `music.c`, `script_tpcmds.c` — 4 each
- `quest.c`, `act_comm.c`, `ban.c`, `channels/channel_service.c`, `gq.c` — 2–3 each
- `note.c`, `utils/buffer.c`, `reputation.c` — 1 each

## Editor residuals (follow-up cleanup only)
The editor-by-editor pass is complete for main list/report builders; remaining counts are likely mixed tab/render helper paths and false positives from this heuristic.
Top residuals by raw count:
- `editors/dungeons/dngedit.c` — 98
- `editors/game_settings/gameedit.c` — 67
- `editors/common/olc_display.c` — 41
- `editors/objects/oedit.c` — 34
- `editors/wilderness/wedit.c` — 30
- `editors/rooms/redit.c` — 27

## Plan
1. Start core hardening in small/medium files (`help.c`, `project.c`) to validate pattern.
2. Move into high-volume core files (`act_info.c`, `act_wiz.c`) in focused chunks.
3. Handle `bprintf`-heavy sites (`blueprint.c`, `dungeon.c`) with the same fail-closed policy.
4. Re-run checklist after each batch and build.

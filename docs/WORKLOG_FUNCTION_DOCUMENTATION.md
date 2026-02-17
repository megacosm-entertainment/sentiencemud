# Worklog: Function Documentation Project

## Purpose
Go through all .c and .h files in /sentience/src and subdirectories, documenting:
- Each function (purpose, parameters, return values)
- Each struct and its members
- To prepare for a proper refactor
- To identify redundant code across multiple files
- To make life easier for developers doing cleanup later

## Progress Tracking

### Legend
- [ ] Not started
- [~] In progress
- [x] Completed

---

## Root /sentience/src Files

### act_*.c Files
- [x] act_comm.c
- [x] act_enter.c
- [x] act_info.c
- [x] act_info2.c
- [x] act_move.c
- [x] act_obj.c
- [x] act_obj2.c
- [x] act_wiz.c

### Core System Files
- [x] comm.c
- [x] connection.c / connection.h
- [x] connection_tcp.c
- [x] connection_tls.c
- [x] connection_websocket.c
- [ ] db.c / db.h
- [ ] db2.c
- [ ] handler.c
- [ ] interp.c / interp.h
- [ ] merc.h
- [ ] recycle.h
- [ ] save.c
- [ ] update.c

### JSON System Files
- [x] json_church.c / json_church.h - Church JSON serialization (8 functions)
- [x] json_chat.c / json_chat.h - Chat room JSON serialization (8 functions)
- [x] json_reserved.c / json_reserved.h - Reserved entities JSON (4 functions)
- [x] json_instance.c / json_instance.h - Instance/ship/dungeon persistence (16+ functions)
- [ ] json_account.c / json_account.h
- [ ] json_area.c / json_area.h
- [ ] json_char.c / json_char.h
- [ ] json_game_settings.c / json_game_settings.h
- [ ] json_persist.c / json_persist.h
- [ ] json_race.c

### Scripting Files
- [ ] script_cmds.c
- [ ] script_commands.c
- [ ] script_comp.c
- [ ] script_const.c
- [ ] script_expand.c
- [ ] script_ifc.c
- [ ] script_lua.c
- [ ] script_mpcmds.c
- [ ] script_opcmds.c
- [ ] script_rpcmds.c
- [ ] script_tpcmds.c
- [ ] script_vars.c
- [ ] scripts.c / scripts.h

### Magic Files
- [ ] magic.c / magic.h
- [ ] magic2.c
- [ ] magic_acid.c
- [ ] magic_air.c
- [ ] magic_astral.c
- [ ] magic_blood.c
- [ ] magic_body.c
- [ ] magic_chaos.c
- [ ] magic_cold.c
- [ ] magic_cosmic.c
- [ ] magic_dark.c
- [ ] magic_death.c
- [ ] magic_earth.c
- [ ] magic_energy.c
- [ ] magic_fire.c
- [ ] magic_holy.c
- [ ] magic_law.c
- [ ] magic_light.c
- [ ] magic_mana.c
- [ ] magic_metal.c
- [ ] magic_mind.c
- [ ] magic_nature.c
- [ ] magic_shock.c
- [ ] magic_soul.c
- [ ] magic_sound.c
- [ ] magic_toxin.c
- [ ] magic_water.c

### Protocol Files
- [ ] protocol.c / protocol.h
- [ ] protocol_layer.c / protocol_layer.h
- [ ] protocol_telnet.c
- [ ] protocol_websocket.c
- [ ] telnet.h

### OLC Files
- [ ] olc.c / olc.h
- [ ] olc_act.c
- [ ] olc_act2.c
- [ ] olc_edit_rsg.c
- [ ] olc_save.c / olc_save.h

### Combat Files
- [ ] fight.c
- [ ] fight2.c
- [ ] shoot.c

### Other Game Files
- [x] alias.c
- [x] async_cache.c / async_cache.h
- [x] auction.c
- [x] autowar.c
- [x] ban.c
- [x] bit.c
- [x] blueprint.c
- [x] boat.c
- [x] chat_rooms.c
- [x] church.c
- [ ] const.c
- [ ] drunk.c
- [ ] dungeon.c
- [ ] editor.c
- [ ] effects.c
- [ ] events.c
- [ ] gq.c
- [ ] help.c
- [ ] house.c
- [ ] html.c
- [ ] hunt.c
- [ ] invasion.c
- [ ] log.c / log.h
- [ ] lookup.c
- [ ] mail.c
- [ ] mccp.c
- [ ] mem.c
- [ ] mount.c
- [ ] msgqueue.c
- [ ] music.c
- [ ] nanny.c
- [ ] note.c
- [ ] project.c
- [ ] quest.c
- [ ] redis_cache.c / redis_cache.h
- [ ] scan.c
- [ ] secret.c / secret.h
- [ ] sha256.c / sha256.h
- [ ] skills.c
- [ ] special.c
- [ ] staff.c
- [ ] stats.c
- [ ] storage.c
- [ ] string.c
- [ ] strings.h
- [ ] tables.c / tables.h
- [ ] test_integration.c
- [ ] tls.c
- [ ] trade.c
- [ ] treasuremap.c
- [ ] weather.c
- [ ] wilds.c / wilds.h
- [ ] debug.h

---

## Subdirectory: account/
- [ ] account_notes.c
- [ ] auth.c / auth.h
- [ ] auth_migrate.c / auth_migrate.h
- [ ] otp.c

---

## Subdirectory: editors/
- [ ] common.c / common.h
- [ ] areas/aedit.c
- [ ] blueprints/bpedit.c
- [ ] blueprints/bsedit.c
- [ ] commands/cmdedit.c
- [ ] dungeons/dngedit.c
- [ ] game_settings/gameedit.c
- [ ] help/hedit.c
- [ ] mobiles/medit.c / medit.h
- [ ] objects/oedit.c
- [ ] projects/pedit.c
- [ ] random_strings/rsgedit.c
- [ ] reserved_vnums/reserved.c / reserved.h
- [ ] rooms/redit.c
- [ ] scripting/olc_mpcode.c
- [ ] ships/shedit.c
- [ ] socials/socialedit.c
- [ ] tokens/tedit.c
- [x] wilderness/vledit.c *(removed — folded into wedit.c as VLinks tab)*
- [ ] wilderness/wedit.c

---

## Subdirectory: io/
- [ ] common.c / common.h

---

## Subdirectory: nanny/
- [ ] nanny.h
- [ ] nanny_auth.c / nanny_auth.h
- [ ] nanny_menus.c / nanny_menus.h
- [ ] nanny_states.h
- [ ] nanny_utils.c / nanny_utils.h

---

## Subdirectory: tests/
- [ ] framework/test_framework.c / test_framework.h
- [ ] framework/test_loader.c
- [ ] integration/wnum_tests.c
- [ ] unit/test_wnum_standalone.c

---

## Documentation Format

All function documentation uses Doxygen-style doc blocks. The standard format is:

```c
/**
 * function_name - Brief one-line description
 *
 * Longer description explaining what the function does, any important
 * behavior notes, side effects, or context needed to understand usage.
 * Multiple paragraphs are fine for complex functions.
 *
 * @param param1  Description of first parameter
 * @param param2  Description of second parameter
 * @return        Description of return value (omit for void functions)
 */
```

### Guidelines:
- **Brief line**: Always use `function_name - description` format (dash separator)
- **Description**: Focus on *what* and *why*, not implementation details
- **Parameters**: Align descriptions for readability, use consistent terminology
- **Return values**: Be specific about success/failure cases, NULL conditions
- **Side effects**: Note if function modifies globals, triggers saves, logs, etc.
- **Player commands**: Document syntax, permissions, and user-facing behavior
- **Static functions**: Still document, but can be briefer

### Common patterns:
- `do_*` functions: Player commands - document syntax and permissions
- `*_update` functions: Tick/pulse handlers - note frequency and what triggers them
- `load_*/save_*` functions: Persistence - note file format and error handling
- `is_*/can_*/has_*` functions: Predicates - clearly state what conditions return true

---

## Notes on "Moved:" Comments
Some files contain `/* Moved: <some other file>.c */` notations. These were from a planned refactor that **never happened** - the functions were never actually moved. When documenting these, we should:
1. Note that the move was planned but not executed
2. Either remove the comment or incorporate it into documentation noting the intended destination

---

## Session Log

### 2026-01-30
- Created this tracking document
- Starting with act_comm.c (currently open in IDE)
- Completed act_comm.c (~60 functions documented)
- Completed act_enter.c (2 functions documented)
- Completed act_info.c (~50+ functions, 4 global arrays documented)
- Completed act_info2.c (~10 functions documented)
- Completed act_move.c (~50 functions, 3 global arrays documented)
- Completed act_obj.c (~65 functions documented - object manipulation, shops, crafting)
- Completed act_obj2.c (~15 functions documented - deposits, lore, tattooing, catalysts)

### 2026-01-31
- Completed act_wiz.c (~110+ functions documented - wizard/admin commands)
  - Large file (~12,800 lines) covering staff commands
  - Includes: set commands (mset, oset, rset, tkset, accset, sset, chset, tset)
  - Includes: visibility (invis, incognito, holylight, holywarp, holyaura, vislist)
  - Includes: object/mob search (*where commands with MXP links)
  - Includes: account management (pwreset, mfareset, acctlink, acctunlink)
  - Includes: cache system (cachestats, cacheinfo, cachedump, cacheload, cachejobs, cachestop)
  - Includes: many utility functions (force, transfer, goto, sockets, etc.)
  - Documented TRIG_TOKEN_GIVEN, TRIG_TOKEN_REMOVED triggers for do_token
  - Documented TRIG_REPOP triggers for do_areset
- Completed alias.c (3 functions documented - alias substitution system)
- Completed async_cache.c (20 functions documented - threaded cache operations)
  - Background worker thread for non-blocking Redis/disk operations
  - Job queue with status tracking (QUEUED/RUNNING/COMPLETE/FAILED)
  - Operations: dump (Redis→disk), load (disk→Redis), invalidate
  - Thread-safe statistics and job management API
- Completed auction.c (3 functions documented - player item auction system)
- Completed autowar.c (7 functions documented - automated PvP war system)
  - Added tech debt item #16 for war type naming review (genocide/jihad terminology)
- Completed ban.c (7 functions documented - site banning system with wildcards)
- Completed bit.c (~30 functions documented - bit manipulation utilities)
  - Core flag/stat lookup functions (flag_value, flag_string, is_stat)
  - Updated flag_type struct documentation in tables.h
  - Bit-to-name converters for all flag categories (affect, extra, act, comm, imm, res, vuln, wear, form, part, weapon, cont, off, channel)
  - Advanced flagbank utilities (bitvector_lookup, bitmatrix_lookup, bitmatrix_isset)
  - String generators for multi-bank flags (bitvector_string, bitmatrix_string, flagbank_string)
- Completed blueprint.c (~70 functions documented - procedural dungeon/instance system)
  - Blueprint and section loading/saving (load_blueprints, save_blueprints, etc.)
  - Blueprint/section lookup functions (get_blueprint, get_blueprint_section, etc.)
  - Instance creation and lifecycle (create_instance, update_instance, extract_instance)
  - Section cloning with room/exit/portal fixup (clone_blueprint_section)
  - Static mode layout generation (generate_static_instance)
  - OLC editors (bsedit, bpedit, do_bsedit, do_bpedit, etc.)
  - Instance save/load for persistence (instance_save, instance_load)
  - Player ownership management (instance_addowner_player, instance_isowner_player, etc.)
  - Utility functions (instance_echo, instance_random_room, get_room_instance)
- Completed boat.c (~100+ functions documented - ship/sailing mechanics system)
  - Very large file (~8350 lines) covering all ship-related functionality
  - Global variables and crew skill defines documented
  - Navigation system: set_seek_point, ship_seek_point, steering functions
  - Steering: steering_calc_heading, steering_update, steering_movement
  - Ship templates: load_ship_index, save_ship_index, load_ships, save_ships
  - Runtime instances: create_ship, extract_ship, ship_load, ship_save
  - Movement: ship_set_move_steps, move_ship_success, ship_move_update
  - Update loops: ship_pulse_update, ships_pulse_update, ship_tick_update
  - Reference resolution: resolve_ships, resolve_ships_player, detach_ships_*
  - Player commands: do_ship (main dispatcher), do_ship_steer, do_ship_sails
  - Airship-specific: do_ship_engines, do_ship_land, do_ship_launch
  - Navigation: do_ship_navigate, do_ship_waypoints, do_ship_routes
  - Crew: do_ship_crew, crew_skill_improve, crew_skill_rating
  - Utility: ship_echo, ship_echoaround, ship_autosurvey, get_ship_location
  - Combat: do_ship_scuttle, do_ship_aim (NYI), do_ship_chase (NYI)
  - Ship management: do_ships (admin), do_ship_christen, do_ship_flag
  - Key/ownership: do_ship_keys, ship_isowner_player, ship_special_key_*
  - Shipyard: is_shipyard_valid, get_shipyard_location, purchase_ship
  - OLC editor: shedit, do_shedit, do_shshow, do_shlist, shedit_table
- Completed chat_rooms.c (~21 functions documented - chat dimension system)
  - do_chat dispatcher and all subcommands (enter, exit, list, join, etc.)
  - Chat room creation (chat_create) with permanent/temporary modes
  - Operator management: is_op, chat_add_op, chat_rem_op, do_chat_op
  - Ban system: do_chat_ban, chat_add_ban, chat_remove_ban
  - Room management: do_chat_topic, do_chat_delete, do_chat_password
  - Admin: do_chat_setfounder, do_chat_show
  - Persistence: write_chat_rooms, read_chat_rooms (widevnum format)
- Completed church.c (~90+ functions documented - player organization/guild system)
  - Very large file (~8900 lines) covering complete church/guild functionality
  - Command dispatcher: do_church, church_command_table, show_church_commands
  - Membership: do_chadd, do_chrem, remove_member, do_chgohall
  - Communication: do_chrules, do_chmotd, do_chtalk, do_chgtalk
  - Economy: do_chdeposit, do_chbalance, do_chwithdraw
  - Creation: do_chcreate, do_chdisband
  - Ranks: add_church_rank, find_rank_by_number, get_chrank, do_chranks
  - Permissions: has_church_permission, do_chrankperm, do_chuserperm
  - Treasure rooms: create_church_treasure_room, can_access_treasure_room
  - Treasure room access: add_rank_to_treasure_room, remove_rank_from_treasure_room
  - Member management: do_chsetmemberrank, can_modify_church_member
  - Ownership: is_church_owner, is_church_leader, do_chowner
  - Logging: add_church_log_entry, display_church_logs, string_end_chlog
  - PK toggle: chtoggle_complete
  - Persistence: save_church, load_church, write_church, fread_church
  - File tracking: has_processed_file, add_to_processed_files
  - Utility: find_rank_by_name, new_church_rank_uid, cmp_church_uid
  - Note: Sex-based rank titles (title_male/title_female) need migration to pronoun system

### 2026-02-01
- Finished church.c documentation (from add_rank_to_treasure_room to end of file)
  - Treasure room functions: add_rank_to_treasure_room, remove_rank_from_treasure_room, create_church_treasure_room
  - Member management: do_chsetmemberrank, is_church_owner, can_modify_church_member
  - Ownership transfer: do_chowner
  - Personal permissions: do_chuserperm
  - Rank utilities: find_rank_by_name, new_church_rank_uid
  - File processing helpers: has_processed_file, add_to_processed_files
  - Log system: add_church_log_entry, chtoggle_complete, string_end_chlog, display_church_logs, is_meta_category
  - Sorting: cmp_church_uid
- Added "Documentation Format" section to this worklog for reference in future sessions
- Completed comm.c (~50+ functions documented - core I/O and communication system)
  - File header updated with module overview and data flow documentation
  - Global variables: All 30+ globals documented with Doxygen-style comments
  - Log redirection: RedirectSTDOUT, RedirectSTDERR, RedirectOutput, CleanupSTDOUT, CleanupSTDERR, CleanupLogs, check_logfile
  - Startup/options: parse_options, detect_test_mode_args, main (entry point with full boot sequence)
  - Socket initialization: init_socket (TCP), init_tls_socket (TLS with OpenSSL)
  - Main loop: game_loop (select-based I/O multiplexing, handshake handling, timing)
  - Descriptor management: init_descriptor, close_socket
  - Input processing: read_from_descriptor, read_from_buffer
  - Output processing: process_output, bust_a_prompt (with @refactor note for named field syntax)
  - Buffer I/O: write_to_buffer, write_to_descriptor, write_to_descriptor_2
  - Character output: send_to_char, send_to_char_bw, page_to_char, page_to_char_bw, show_string, printf_to_char
  - Name validation: check_parse_name
  - Reconnection: check_reconnect, complete_reconnect, reconnect_char, check_playing, stop_idling
  - Act system: act_new (comprehensive variable substitution documentation)
  - Utility: stptok, room_echo, echo_around, show_form_state
  - Validation: acceptablePassword
  - Timers: update_pc_timers (20+ timer types documented)
  - Character creation: add_possible_races, add_possible_subclasses
  - Connection tracking: connection_add, connection_remove
  - SSL management: init_ssl_cleanup_queue, SSL_CLEANUP_DATA, add_ssl_ctx_to_cleanup, process_ssl_cleanup_queue, refresh_ssl_context
- Completed connection abstraction layer documentation (all 4 files):
  - connection.h: File header with vtable pattern explanation, enums (connection_type_t, connection_state_t), connection_vtable struct with method signatures, connection_t base struct, factory function prototypes, inline wrapper functions
  - connection.c: File header, connection_set_nonblocking(), connection_state_name(), connection_type_name()
  - connection_tcp.c: File header, connection_tcp_t struct, tcp_vtable, connection_tcp_create(), tcp_read(), tcp_write(), tcp_process_handshake(), tcp_close(), tcp_free(), tcp_is_secure(), tcp_get_protocol_name()
  - connection_tls.c: File header, external SSL_CTX reference, connection_tls_t struct, tls_vtable, log_ssl_error(), connection_tls_create(), tls_read(), tls_write(), tls_process_handshake(), tls_close(), tls_free(), tls_is_secure(), tls_get_protocol_name()
  - connection_websocket.c: File header with RFC 6455 overview, WS_OPCODE_* defines, ws_handshake_state_t enum, ws_state_t struct (frame parsing state machine), connection_websocket_t struct, ws_base64_encode(), generate_accept_key(), process_ws_handshake(), connection_websocket_create(), ws_unmask_payload(), ws_parse_frame(), ws_read(), ws_write(), ws_process_handshake(), ws_close(), ws_free(), ws_is_secure(), ws_get_protocol_name(), connection_websocket_tls_create(), wss_process_handshake(), wss_read(), wss_write(), wss_close(), wss_is_secure(), wss_get_protocol_name()

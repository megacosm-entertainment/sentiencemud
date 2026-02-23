# WORKLOG: Channel Helperization & Parity Review Prep

Date: 2026-02-23  
Branch: `feature/widevnum-migration`

## Scope
Document recent boilerplate-reduction work in the new channel system, and provide a concrete checklist for the upcoming final parity review against old channel behavior.

---

## 1) Refactors Completed

### 1.0 Dispatcher policy flag: string-editor guard
Added a new per-channel flag to make accidental string-editor command text blocking configurable instead of hardcoded:
- `CHANNEL_FLAG_GUARD_STR_EDIT_CMDS`

Implemented behavior:
- Dispatcher now enforces the guard when this flag is set.
- `gossip` now uses dispatcher-driven guard behavior (legacy inline checks removed).
- The flag is editable in `cedit` as `guard_str_edit`.

Default wiring:
- `gossip` defaults now include this flag so existing behavior is preserved.

### 1.7 Channel command metadata parity (`sethelp` + `summary`)
Added cmd-style metadata support to channel definitions:
- `help_keywords`
- `summary`

Implemented in:
- `CHANNEL_DEF_DATA` schema + registry JSON load/save.
- `cedit` subcommands:
  - `sethelp <keywords|#index|clear>`
  - `summary <text|clear>`

`commands` integration:
- `do_commands` now appends dynamic channel commands in the COMM section when:
  - the command name is not already in static command definitions, and
  - the channel is currently available to the player via channel requirements/scope checks.
- Dynamic channel entries use the same help/summary hint behavior as regular commands.

### 1.8 Channel alias arrays (`aliases`)
Added multi-alias support to channel definitions so shorthand/legacy command aliases can be carried by channel config:
- New schema fields:
  - `alias_count`
  - `aliases[]`
- JSON persistence now loads/saves an `aliases` array per channel.
- Runtime channel resolution now matches aliases for exact and prefix lookups.
- OOC command detection in `interp.c` now respects aliases.

Editor support:
- `cedit aliases` (list)
- `cedit aliases add <alias>`
- `cedit aliases del <alias>`
- `cedit aliases clear`

Default legacy shorthand aliases are seeded for built-ins:
- `gossip` → `.`
- `gtell` → `;`
- `say` → `'`
- `immtalk` → `:`

### 1.9 Preference parity migration: announcements + hints
Moved these legacy channel toggles away from direct `COMM_NO*` bit handling and onto channel preferences:
- `do_announcements` now toggles `channel_announce` preference.
- `do_hints` now toggles `channel_hints` preference.

Delivery parity update:
- `crier_announce()` now checks `pref_check_channel(victim, "announce")` rather than `COMM_NOANNOUNCE` directly.

This removes another blocker for deprecating old comm-bit channel paths and aligns behavior with the dynamic channel preference model.

### 1.10 Preference parity migration: tells
Harmonized tells on/off state and delivery checks with channel preferences:
- `do_tells` now toggles `channel_tells` preference.
- `do_channels` now displays tells state from `pref_check_channel(ch, "tells")`.
- `do_tell` receive gating now checks `pref_check_channel(victim, "tells")`.
- `channel_deliver_tell_legacy()` receive gating now checks `pref_check_channel(recipient, "tells")`.

Compatibility bridge:
- Added legacy COMM bit fallback in `pref_check_channel()` when no explicit preference exists.
- Added `tells` to `channel_mute_table` so preference-application and legacy migration paths stay aligned.

### 1.11 Toggle-only command cleanup
Reduced residual boilerplate in legacy toggle-only handlers by adding a shared helper in `act_comm.c`:
- `toggle_channel_preference(ch, channel_id, msg_on, msg_off)`

Converted to helper-driven implementations:
- `do_tells`
- `do_hints`
- `do_announcements`

This keeps behavior unchanged while making final deprecation/replacement of these wrappers lower risk.

### 1.12 Toggle-only channels moved to dispatcher ownership
Completed the next deprecation step by moving remaining toggle-only wrappers onto the dynamic channel path:
- `do_hints` now delegates to `dispatch_dynamic_channel_command(ch, "hints", argument)`
- `do_tells` now delegates to `dispatch_dynamic_channel_command(ch, "tells", argument)`
- `do_announcements` now delegates to `dispatch_dynamic_channel_command(ch, "announcements", argument)`

Dispatcher policy update:
- `dispatch_dynamic_channel_command()` now enforces `CHANNEL_FLAG_TOGGLE_ONLY` for publish attempts.
- Non-empty arguments on toggle-only channels now return: `You cannot talk over the <channel> channel.`

Registry defaults update:
- Added built-in defs for `announce`, `hints`, and `tells`.
- Added default `CHANNEL_FLAG_TOGGLE_ONLY` assignment for those IDs.

Cleanup:
- Removed the now-unused local helper `toggle_channel_preference(...)` from `act_comm.c`.

Validation:
- Rebuilt via `./build` and confirmed no diagnostics in touched files.

### 1.13 Immtalk parity migration (legacy COMM_NOWIZ deprecation)
Moved immortal channel toggle/display behavior to the same dispatcher + preference model used by other dynamic channels:
- `do_immtalk` now delegates to `dispatch_dynamic_channel_command(ch, "immtalk", argument)`.
- `do_channels` god-channel status now reads from `pref_check_channel(ch, "immtalk")`.

Compatibility bridge:
- Added `immtalk` to `channel_mute_table` mapped to `COMM_NOWIZ` for legacy fallback in `pref_check_channel()`.
- Updated `channel_can_deliver_to_descriptor()` to map `COMM_NOWIZ` to canonical channel preference `"immtalk"`.

Result:
- Removes another direct command-path dependency on legacy COMM bits while preserving legacy fallback behavior for unmigrated characters/accounts.

### 1.14 Immtalk access policy moved to channel definition requirements
Removed remaining hard-coded immortal recipient gating for `immtalk` from legacy delivery loop and moved policy ownership to channel registry defaults:
- `channel_default_publish_requirements_for_id("immtalk")` now requires `staff_rank >= immortal`.
- `channel_default_subscribe_requirements_for_id("immtalk")` now requires `staff_rank >= immortal`.

Delivery simplification:
- `channel_deliver_immtalk_legacy()` no longer has a bespoke `IS_IMMORTAL(victim)` branch.
- Recipient allow/deny now flows through the shared `channel_send_formatted_to_recipient()` requirements + preference checks.

Result:
- One less hard-coded policy branch in service delivery, improving parity between legacy and dynamic channel paths.

### 1.15 Helper access policy moved to requirements path
Completed the same policy de-hardcoding for helper channel behavior:
- Removed helper-specific toggle-on guard from `dispatch_dynamic_channel_command()` and replaced it with generic subscribe-requirements evaluation.
- Removed helper-specific recipient gating branch from `channel_deliver_helper_legacy()`.

Behavior now flows through shared policy points:
- Toggle-on eligibility: `requirements_evaluate_text(def->subscribe_requirements, ...)`
- Recipient eligibility: shared `channel_send_formatted_to_recipient()` checks (requirements + prefs + penalties)

Result:
- Helper access control is now owned by channel definitions (`publish_requirements` / `subscribe_requirements`) instead of hard-coded special cases.

### 1.16 Tell/GTell sender gating compatibility bridge
Reduced direct legacy `COMM_NOTELL` branching in command handlers while preserving behavior:
- Added centralized sender-revocation helper in `act_comm.c` that checks, in order:
  - channel-specific mute penalty (`has_channel_penalty(account, channel_id, ...)`)
  - legacy `PENALTY_NOTELL`
  - legacy `COMM_NOTELL` fallback
- `do_tell` now gates via `channel_sender_revoked(ch, "tell")`.
- `do_gtell` now gates via `channel_sender_revoked(ch, "gtell")`.
- `do_channels` “You cannot use tells.” status now uses the same helper path rather than a direct `COMM_NOTELL` check.

Result:
- Tell-family sender gating now prefers channel-policy primitives with a single compatibility bridge point, shrinking legacy spread and making final removal of `COMM_NOTELL` references lower risk.

### 1.17 Global channel revocation compatibility bridge
Applied the same centralization pattern to global channel revocation checks:
- Added `channel_global_revoked(ch)` in `act_comm.c`.
- Replaced duplicated `COMM_NOCHANNELS` / `PENALTY_NOCHANNELS` branches in:
  - `dispatch_dynamic_channel_command()`
  - `can_speak_channels()`
  - `do_channels` status output

Result:
- Legacy global-channel revocation logic is now isolated to a single helper path, reducing duplication and simplifying future removal of direct `COMM_NOCHANNELS` handling.

### 1.18 Shared channel policy module extraction
Moved command/service policy helpers out of `act_comm.c` into a shared module:
- Added:
  - `src/channels/channel_policy.h`
  - `src/channels/channel_policy.c`
- Added to both build systems:
  - `src/CMakeLists.txt`
  - `src/Makefile`

API extracted:
- `channel_policy_sender_revoked(ch, channel_id)`
- `channel_policy_global_revoked(ch)`

Adoption:
- `act_comm.c` now uses shared policy API and no longer owns local revocation helper implementations.
- `channel_service.c` now uses `channel_policy_sender_revoked()` for both broadcast and directed send prechecks.

Result:
- Command and service layers now consume the same sender/global revocation policy API, reducing drift and centralizing compatibility fallback behavior.

### 1.19 Tell path trigger regression corrected
During parity auditing, tell delivery was briefly wired to fire `TRIG_SPEECH` through the service path.
That behavior has been reverted to match current `do_tell` semantics:
- `do_tell` now uses `channel_service_send_directed(...)` (no trigger payload API).
- Tell legacy delivery no longer calls `p_act_trigger(...)`.

Behavior preserved:
- AFK buffering and linkdead buffering semantics unchanged.
- Reply pointer update semantics unchanged.
- Sender/recipient visible tell formatting unchanged.

### 1.1 New common helper module
Added shared channel helper files:
- `src/channels/channels_common.h`
- `src/channels/channels_common.c`

### 1.2 Build wiring
Added `channels/channels_common.c` to both build systems:
- `src/CMakeLists.txt`
- `src/Makefile`

### 1.3 Scope + mode + modifier helper consolidation
Centralized and reused:
- Scope conversions:
  - `channel_scope_to_name`
  - `channel_scope_to_display_name`
  - `channel_scope_from_name`
- Filter mode conversions:
  - `channel_filter_mode_to_name`
  - `channel_filter_mode_from_name`
- Modifier lookups + metadata:
  - `channel_modifier_flag_from_name`
  - `channel_modifier_flag_table`
  - `channel_text_modifier_mask`
  - `channel_text_modifier_fallback_order`

Callsites updated:
- `src/channels/channel_registry.c`
- `src/channels/channel_service.c`
- `src/editors/channels/cedit.c`

### 1.4 Topic/scope topic-builder consolidation
Moved shared topic builders into common helpers:
- `channel_build_area_scope_topic`
- `channel_build_region_scope_topic`
- `channel_build_group_scope_topic`
- `channel_build_room_scope_topic`
- `channel_build_instance_scope_topic`
- `channel_build_dungeon_scope_topic`
- `channel_build_entity_topic`
- `channel_build_church_scope_topic`

Callsite updates:
- `src/channels/channel_service.c`

### 1.5 Topic pattern expansion consolidation
Moved topic pattern expansion from service into common helper:
- `channel_topic_expand_pattern`

Callsite update:
- `src/channels/channel_service.c`

### 1.6 Validation status
Build validation done repeatedly via project build script (`./build`) after each extraction phase.

---

## 2) High-Value Next Helperization Targets (Similar, not exact duplicates)

These are strong candidates for reducing boilerplate while preserving behavior.

### A. Legacy broadcast channel delivery fan-out
Current state: many per-channel functions with very similar loops and gating.
Examples include:
- `channel_deliver_ooc_legacy`
- `channel_deliver_gossip_legacy`
- `channel_deliver_quote_legacy`
- `channel_deliver_flame_legacy`
- `channel_deliver_helper_legacy`
- `channel_deliver_music_legacy`
- `channel_deliver_immtalk_legacy`
- `channel_deliver_yell_legacy`
- `channel_deliver_gtell_legacy`

Potential helper design:
- Generic broadcaster taking:
  - `channel_id`
  - recipient predicate callback
  - iteration mode (`descriptor_list`, group members, room members)
  - comm-block flags and visibility behavior

### B. Targeted in-room delivery pair
Current state:
- `channel_deliver_whisper_legacy`
- `channel_deliver_sayto_legacy`

Potential helper design:
- `channel_deliver_targeted_room_legacy(channel_id, sender, target, text, options)`

### C. Report/history ring management
Current state: parallel logic patterns for append/copy/load/save/compact/find in report and history rings.

Potential helper design:
- Internal ring-buffer utility helpers parameterized by record type and filtering rule.

### D. Delivery policy consolidation
Current state: policy checks distributed across multiple narrow functions.
Examples:
- notells/quiet/ignore checks
- ban checks
- preference checks
- contextual history visibility checks

Potential helper design:
- Unified `channel_delivery_policy_eval(...)` returning reason code + flags.

### E. Transport backend mechanical repetition
Current state: local and redis backends repeat queue/state/lifecycle patterns.

Potential helper design:
- Shared transport queue utility
- Shared backend health/state transition utility

### F. Moderation command parsing common flow
Current state: `chanmute` / `chanban` / `chanwarn` share parse-normalize-apply pattern with action-specific branches.

Potential helper design:
- Shared parse phase and an action table for penalty application behavior.

---

## 3) Final Parity Review Checklist (Old vs New Channel Behavior)

Use this checklist in the final audit prompt.

### 3.1 Message delivery semantics
- Sender self-echo behavior parity per channel
- Recipient visibility parity (`pers`, invis/wizi handling)
- AFK buffering/reply behavior parity
- Ignore/notells/quiet interactions parity (including staff bypass rules)

### 3.2 Scope/routing parity
- Global, area, region, room, group, church, instance, dungeon, direct entity
- Isolated area/instance/dungeon behavior
- Topic key generation consistency for all scopes

### 3.3 Formatting parity
- `fmt_self`, `fmt_receiver`, `fmt_notvict` fallback behavior
- Church token expansion parity (`$church_colour1`, `$church_colour2`, `$sender_flag`)
- Legacy say punctuation parsing/exclaim/ask behavior parity

### 3.4 Moderation/filter parity
- Warn/mute/ban semantics
- Filter modes (`allow/redact/block/review`) and review stream behavior
- Staff report creation/ack/purge/trim behavior

### 3.5 Persistence parity
- Registry load/save stability
- History and report ring persistence and recovery
- Runtime reload behavior when channel config changes

### 3.6 Backend parity
- Legacy iterative vs local vs redis behavior equivalence for key gameplay outcomes
- Inbound/outbound queue handling and failure behavior

### 3.7 Command-level parity matrix
Verify old command behavior vs new path for:
- `say`, `whisper`, `sayto`, `tell`
- `gossip`, `ooc`, `quote`, `flame`, `helper`, `music`, `immtalk`, `yell`, `gtell`, `chtalk`

---

## 4) Suggested order for next implementation work
1. Collapse legacy broadcast delivery fan-out (Target A).  
2. Collapse targeted room delivery pair (Target B).  
3. Introduce delivery policy evaluator (Target D).  
4. Refactor report/history ring internals (Target C).  
5. Re-run parity matrix and behavior tests.

---

## Notes
- Keep behavior-preserving refactors small and incremental.
- Rebuild with `./build` after each extraction.
- Prefer helper APIs with explicit parameters over hidden globals where practical.

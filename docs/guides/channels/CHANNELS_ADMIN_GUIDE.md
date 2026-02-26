# Admin Guide: Channel System

This document covers channel administration, moderation, filtering, review tooling, and the runtime publish/subscribe model.

## Overview

The channel system is definition-driven through the channel registry and channel service.

Core components:

- Registry: `src/channels/channel_registry.c` / `channel_registry.h`
- Service/runtime: `src/channels/channel_service.c` / `channel_service.h`
- Policy checks: `src/channels/channel_policy.c` / `channel_policy.h`
- Filtering engine: `src/channels/channel_filter.c` / `channel_filter.h`
- Review queue integration: `src/channels/channel_review.c` / `channel_review.h`
- Moderation commands: `src/channels/channel_moderation.c` / `channel_moderation.h`

Transport backends:

- legacy iterative
- local queue backend
- redis backend (with pubsub/history support)

## Channel Definitions and Editor (`cedit`)

Open editor:

- `cedit <channel_id>`
- `cedit create <channel_id>`

Persistence:

- channel definitions serialize to `data/system/channels.json`

### High-value `cedit` commands

- `show`
- `name <text>`
- `scope <scope>`
- `topic <pattern>`
- `flags <playerflags|chanflag> [on|off]`
- `modifiers <...>`
- `modorder <comma-list>`
- `aliases ...`
- `format ...`
- `history ...`
- `requirements ...`
- `filter ...`
- `review ...`
- `modadd <player>` / `moddel <player>` / `modlist`
- `punish <warn_threshold|mute_minutes> <value>`
- `sethelp <keywords|#index|clear>`
- `summary <text|clear>`
- `persistent <on|off>`
- `save`

## Publish/Subscribe Runtime Model

### Send path

Typical flow in `channel_service_send*`:

1. Resolve channel definition
2. Evaluate sender/global policy revocation
3. Evaluate channel requirements (publish)
4. Apply text modifiers (if configured)
5. Evaluate channel filter decision
6. Handle block/redact/review
7. Deliver via backend publish (or local fallback)
8. Persist history/report metadata as appropriate

### Delivery and subscriptions

- Service computes effective subscription topics from context.
- Topic set is synchronized on pulses.
- Local backend routes through in-process queue; redis backend routes via Redis pubsub.

### Scope examples

- `GLOBAL`
- `AREA`
- `REGION`
- `ROOM_WV`
- `DIRECT_ENTITY`
- `GROUP_ID`
- `CHURCH_ID`
- `INSTANCE_ID`
- `DUNGEON_ID`

## Channel Flags

Definition flags include behavior controls such as:

- `honor_pers`
- `honor_wizi`
- `allow_ban`
- `is_ooc`
- `respect_silence`
- `ignore_quiet`
- `guard_str_edit`
- `toggle_only` (set in channel flags bitset)

Practical effects:

- identity rendering and invis handling
- silence/quiet interaction
- whether accidental string-editor command text is blocked
- whether channel is receive-toggle only (no message publish)

## Requirements (Publish/Subscribe Gates)

Each channel can define JSON requirements separately for publish and subscribe.

Editor:

- `requirements publish <json|clear>`
- `requirements subscribe <json|clear>`
- `requirements clear <publish|subscribe|all>`
- `requirements show`

Example: immortal-only channel

```json
{"staff_rank":{"op":">=","value":"immortal"}}
```

Example: helper-or-immortal

```json
{"any_of":[{"plr_flag":"helper"},{"staff_rank":{"op":">=","value":"immortal"}}]}
```

## Moderation and Penalties

Staff commands:

- `chanmute <char> [channel|*] <duration> [reason]`
- `chanban <char> [channel|*] [reason]`
- `chanwarn <char> [channel|*] [reason]`
- `chanunmute <char> [channel|*]`
- `chanpenalties <char>`

Notes:

- `channel` may be a specific id or `*` for all channels.
- `chanban` is effectively permanent channel mute.
- warning records are stored but do not directly mute.

Tell/global compatibility:

- tell-family and global revocation checks are centralized in channel policy helpers.

## Filtering

Filtering exists on both channel-side moderation and player-side receive preferences.

### Channel-side filters (definition-driven)

Configured in `cedit filter`:

- enable/disable + mode: `allow|redact|block|review`
- simple text match and/or regex pattern

Commands:

- `filter <on|off> [allow|redact|block|review]`
- `filter simple <text>`
- `filter regex <pattern>`
- `filter clear <simple|regex|all>`

Marker shortcuts currently supported by filter engine:

- `[block]`
- `[redact]`
- `[review]`

Decision behavior:

- `allow`: normal delivery
- `redact`: deliver redacted payload
- `block`: do not deliver message
- `review`: deliver, and queue report metadata

### Player-side receive filters

Delivery checks recipient preference filters:

- global: `filter_simple`, `filter_regex`
- per-channel: `filter_<channel>_simple`, `filter_<channel>_regex`

If matched, recipient does not see the message.

## Review / Report System

### Automatic review queue integration

When filter result requests review, service appends review metadata and can notify staff.

Review stream defaults to:

- `audit:filtered_messages`

Override per channel via:

- `review stream <stream_name>` in `cedit`

### Player reports from history

Players can submit reports through:

- `history <channel> report <message-id> [notes]`

Service includes surrounding context in report payload.

### Staff review command (`rview`)

- `rview list [limit]`
- `rview listall [limit]`
- `rview read <report-id|index>`
- `rview ack <report-id|index> [note]`
- `rview purge acked`
- `rview trim <max-entries>`

## History

History interfaces:

- `history <channel>`
- `history <channel> info <index>`

Stored metadata can include report linkage.

Context-sensitive channels only show relevant history for the viewer context.

## Operations Checklist

When introducing/updating a channel:

1. Define/edit with `cedit`
2. Set scope/topic/aliases
3. Configure requirements for publish + subscribe
4. Configure flags/modifiers/format
5. Configure filter/review behavior
6. Save and validate in-game with:
   - `channels`
   - `history <channel>`
   - moderation flow (`chan*`)
   - review flow (`rview`)

## Player/Admin Doc Split

- Player-facing behavior and commands: see `CHANNELS_PLAYER_GUIDE.md`
- Admin/editor/moderation/runtime internals: this file

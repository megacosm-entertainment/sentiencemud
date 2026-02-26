# Player Guide: Channels

This guide explains how players use the new channel system, what `channels` shows, and how filtering/reporting works.

## Quick Start

- `channels` shows channels currently available to you in your context.
- Each listed channel shows:
  - `recv`: whether you currently receive that channel (`ON`/`OFF`)
  - `access`: whether you can `post`, `sub`, or both (`post+sub`)
  - `source`: command name used to interact with the channel
- `history <channel>` shows recent messages for that channel.
- `history <channel> report <message-id> [notes]` reports a message to staff review.

## The `channels` Command

`channels` now focuses on channel capability/status and communication-related toggles.

### What you see

- Dynamic list of channels you can currently use in some way:
  - `post` = you can send on it now
  - `sub` = you can receive/toggle it now
  - `post+sub` = both
- Your receive state (`ON` / `OFF`) for each listed channel
- Communication toggles like notify/quiet and related status
- Your current player flag and which channels you display flags on

### What was removed

Prompt and scroll-lines display were removed from `channels` because those belong to the preferences system.

## Channel Receive Preferences

Most channels are controlled through per-channel preferences.

Examples of channel toggles (depending on your permissions/context):

- `gossip`
- `ooc`
- `quote`
- `flame`
- `music`
- `tells`
- `hints`
- `announcements`
- `helper` / `immtalk` / `chtalk` (when eligible)

If a channel is OFF in `channels`, you usually won’t receive it.

## Posting and Context

Some channels are always global; others depend on your current context (group/church/room/instance/dungeon). You may see channels appear/disappear as context changes.

Examples:

- Group-only channels appear when grouped.
- Church channels appear when in a church.
- Context-scoped channels depend on your room/area/instance/dungeon state.

## Player Flags

Your chat flag is separate from whether you *see* other players’ flags.

- `flag <text|none>` sets/removes your own flag text.
- `flag <channel>` toggles whether you see player flags on that channel.

`channels` shows:

- your current flag value
- channels where you enabled flag display
- channels currently available to you that support player flags

## Filtering (Player-Side)

You can suppress incoming messages via preference filters.

Supported preference keys used by channel delivery include:

- global:
  - `filter_simple`
  - `filter_regex`
- per-channel:
  - `filter_<channel>_simple`
  - `filter_<channel>_regex`

Behavior:

- If a message matches one of your filters, it is not shown to you.
- This only affects your own receiving view.

## History and Reporting

Use history tools to inspect and report messages.

### History list

- `history <channel>`

Shows recent entries, including message IDs.

### Detailed entry

- `history <channel> info <index>`

Shows one entry in detail.

### Report message

- `history <channel> report <message-id> [notes]`

Submits report with context to staff review queue.

## Common Status Messages

- `You cannot use channels.`
  - global channel use is revoked (penalty/restriction)
- `You cannot use tells.`
  - tell-family use is revoked
- `Your message was blocked by channel filters.`
  - channel-side moderation filter blocked outgoing message

## Intone Notes

`intone` is object-targeted speech and now goes through channel-service filtering/policy checks:

- may be blocked/redacted/reviewed by channel filters
- respects channel revocation/policy gates


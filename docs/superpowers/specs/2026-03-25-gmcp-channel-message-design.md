# Design: Sentience.Channel.Message Modernization & GMCP Preferences

**Status:** Approved Design
**Date:** 2026-03-25

---

## Problem

The existing `Sentience.Channel.Message` GMCP package in `channel_gmcp.c` works but has
several limitations:

1. **Raw JSON construction** — uses `sprintf` with manual escaping instead of the Jansson
   builder pattern used by all other `Sentience.*` packages
2. **No versioning** — lacks the `_v` field that enables client-side schema migration
3. **No preference control** — GMCP channel data is sent to all GMCP-capable descriptors
   regardless of whether the client wants it
4. **No inline suppression** — web clients receive both GMCP channel data AND inline text,
   causing duplicate content in chat panels
5. **Hardcoded minimap suppression** — the current minimap inline suppression in `act_info.c`
   is tied to GMCP capability rather than user preference
6. **Not registered** — `Sentience.Channel.Message` is not in `Client.Ready.Capabilities`

## Approach

Two changes working together:

**Part A** — Add a `PREF_CAT_GMCP` preference category with three boolean preferences
that control GMCP behavior. Add a bidirectional `Sentience.Client.Preferences` GMCP
package so the web client can read and set these preferences dynamically.

**Part B** — Modernize `channel_gmcp.c` to use Jansson builders with `_v` versioning,
add preference-based filtering, and register in Capabilities.

---

## Part A: GMCP Preferences

### New Preference Category

Add `PREF_CAT_GMCP` (value 4) to `account/preferences.h`. Bump `PREF_CAT_MAX` to 5.

### Preferences

| Key                      | Type | Default (WebSocket) | Default (Telnet) | Description |
|--------------------------|------|---------------------|-------------------|-------------|
| `gmcp_channels`          | bool | true                | false             | Send `Sentience.Channel.Message` GMCP data |
| `gmcp_suppress_channels` | bool | true                | false             | Suppress inline channel text when GMCP is delivering it |
| `gmcp_suppress_minimap`  | bool | true                | false             | Suppress inline minimap when GMCP is delivering Room.Map |

**Resolution order:** character override → account → game-wide default. Connection-type
defaults (WebSocket=true, telnet=false) are seeded as character-level overrides at first
login when no preference exists anywhere in the chain. After seeding, resolution is the
standard three-level lookup — the connection type is not consulted at runtime.

### Accessor Functions

Add to `account/preferences.h`:

```c
bool pref_gmcp_channels(CHAR_DATA *ch);
bool pref_gmcp_suppress_channels(CHAR_DATA *ch);
bool pref_gmcp_suppress_minimap(CHAR_DATA *ch);
```

These are convenience wrappers around `pref_get_bool()` that handle the inheritance chain.
They return the effective value for the character, considering overrides, account prefs,
game defaults, and the connection-type default.

### Default Application

In `nanny.c` (or wherever the character enters the game after login), after
`pref_apply_to_character()`, call `pref_apply_gmcp_defaults(ch)`. This function:

1. For each GMCP pref key, check if any value exists in the inheritance chain
2. If no value exists anywhere, set a character-level preference based on connection type:
   - WebSocket → `true`
   - Telnet/TLS → `false`

This means:
- First-time WebSocket login: all three prefs default ON
- First-time telnet login: all three prefs default OFF
- Returning players: their saved preferences are respected regardless of connection type
- Players who switch connection types keep their saved preferences

### Player Command Integration

Add GMCP preferences to the `prefs` command output under a "GMCP" section. Players can
toggle with `prefs gmcp_channels`, `prefs gmcp_suppress_channels`, `prefs gmcp_suppress_minimap`.

### Sentience.Client.Preferences GMCP Package

Bidirectional package for reading and setting GMCP preferences from the client.

**Server → Client (on login, after capabilities):**
```json
{
  "_v": 1,
  "gmcp_channels": true,
  "gmcp_suppress_channels": true,
  "gmcp_suppress_minimap": true
}
```

**Client → Server (preference update):**
```json
{
  "gmcp_suppress_channels": false
}
```

The client sends only the keys it wants to change. The server validates each key (must be
a known GMCP pref), applies the change as a character-level override, saves the character,
and echoes back the full preference state.

**Server-side handler:** Add a handler in `protocol.c` for incoming
`Sentience.Client.Preferences` messages. Parse the JSON, validate keys against a whitelist
of GMCP pref names, call `pref_set_bool()` for each, save the character, then send the
full preference state back as confirmation.

---

## Part B: Channel.Message Modernization

### Builder Pattern

Add a pure JSON builder in `gmcp_sentience.c`:

```c
typedef struct {
    const char *channel;      /* Channel ID (e.g. "gossip", "say") */
    const char *sender;       /* Sender name (empty string for system messages) */
    const char *text;         /* Message text (color-stripped) */
    long        timestamp;    /* Unix timestamp */
    const char *tell_target;  /* Recipient name (NULL if not directed) */
} sentience_channel_message_input_t;

json_t *sentience_build_channel_message(const sentience_channel_message_input_t *input);
```

The builder produces:
```json
{
  "_v": 1,
  "channel": "gossip",
  "sender": "PlayerName",
  "text": "Hello everyone!",
  "timestamp": 1711353600
}
```

For directed channels (tell), adds `"tell_target": "RecipientName"`.

### Preference-Based Filtering

Modify `channel_gmcp_broadcast()` and `channel_gmcp_send_directed()` to check preferences:

1. Check `pref_gmcp_channels(d->character)` — skip if false
2. Check `pref_check_channel(d->character, def->id)` — skip if channel is muted
3. Use `sentience_send_package()` instead of `SendGMCPRaw()`

The `sentience_send_package()` function (currently `static` in `gmcp_sentience.c`) must be
made non-static and declared in `gmcp_sentience.h` so `channel_gmcp.c` can call it.

The existing scope check (`channel_gmcp_recipient_in_scope`) remains unchanged.

### Inline Text Suppression

In `channel_service.c`, add a preference check to the legacy delivery functions. Before
calling `act()` or `channel_send_formatted_to_recipient()` for each recipient, check:

```c
if (pref_gmcp_suppress_channels(victim) && pref_gmcp_channels(victim))
    continue;  /* GMCP is delivering this; skip inline text */
```

This check goes in `channel_can_deliver_to_descriptor()` as an additional filter, since
all legacy delivery functions already call it. If `gmcp_suppress_channels` is on AND
`gmcp_channels` is on, skip the inline delivery — the GMCP path handles it.

**Important:** The suppression only applies when BOTH prefs are true. If `gmcp_channels`
is off but `gmcp_suppress_channels` is on, inline text still shows (there's no GMCP to
replace it).

### Minimap Suppression Update

Replace the current hardcoded GMCP check in `act_info.c` (lines 2601-2610 and 2628-2635)
with preference checks:

**Before (current):**
```c
!(ch->desc && ch->desc->pProtocol &&
  ch->desc->pProtocol->bGMCP &&
  ch->desc->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE])
```

**After:**
```c
!pref_gmcp_suppress_minimap(ch)
```

This is simpler and preference-driven. The `pref_gmcp_suppress_minimap()` accessor handles
the full inheritance chain internally.

### Capabilities Registration

Add `"Sentience.Channel.Message"` and `"Sentience.Client.Preferences"` to the packages
array in `sentience_build_client_ready_capabilities_json()`.

---

## Files Changed

| File | Changes |
|------|---------|
| `account/preferences.h` | Add `PREF_CAT_GMCP`, bump `PREF_CAT_MAX`, declare accessors |
| `account/preferences.c` | Implement accessors, add GMCP defaults, add to `prefs` command display |
| `gmcp_sentience.h` | Add `sentience_channel_message_input_t`, builder declaration |
| `gmcp_sentience.c` | Add builder, add to capabilities, add `Sentience.Client.Preferences` send |
| `channels/channel_gmcp.c` | Use Jansson builder, add pref filtering, use `sentience_send_package()` |
| `channels/channel_gmcp.h` | No changes expected (function signatures stay compatible) |
| `protocol.c` | Add handler for incoming `Sentience.Client.Preferences` GMCP |
| `act_info.c` | Replace hardcoded GMCP check with `pref_gmcp_suppress_minimap()` |
| `channels/channel_service.c` | Add inline suppression check in `channel_can_deliver_to_descriptor()` |
| `nanny.c` | Call `pref_apply_gmcp_defaults()` on login |
| `tests/unit/gmcp_sentience_tests.c` | Add Channel.Message builder tests |
| `tests/data/unit/gmcp_sentience_unit_tests.json` | Add test scenarios |

---

## Testing

### Unit Tests
- `sentience_build_channel_message()` — broadcast format (no tell_target)
- `sentience_build_channel_message()` — directed format (with tell_target)
- `sentience_build_channel_message()` — system message (empty sender)
- Capabilities list includes Channel.Message and Client.Preferences

### Integration Verification
- WebSocket client receives GMCP channel messages for subscribed channels
- WebSocket client does NOT receive GMCP for muted channels
- Telnet client receives inline text (no GMCP by default)
- `prefs gmcp_channels` toggle works
- `prefs gmcp_suppress_channels` suppresses inline text
- `prefs gmcp_suppress_minimap` suppresses inline minimap
- Sending `Sentience.Client.Preferences` from client updates preferences
- Preferences persist across reconnect

---

## Out of Scope

- Channel history replay via GMCP (future feature)
- Per-channel GMCP toggle (single global toggle is sufficient)
- Web client UI rendering (client-side work)
- Mudlet package updates (separate effort)

# Sentience.Link — GMCP Link Abstraction Layer (Phase 2)

## Problem Statement

Sentience's 11 `mxp_*` link functions in `mxp_links.c` generate MXP `<send>` tags for
telnet clients that negotiate MXP support.  WebSocket/web clients receive **no interactive
links at all** — they see plain text where clickable elements should be.  Additionally,
modern terminals (Mudlet 4.18+, iTerm2, Kitty, xterm) support OSC 8 hyperlinks natively
but Sentience doesn't emit them.

Phase 2 adds a transport-agnostic link abstraction: every `mxp_*` function routes through
a 4-way branch based on connection capabilities, and a new `Sentience.Link.List` GMCP
package delivers rich link metadata to web clients.

## Goals

1. WebSocket clients receive structured link data (GMCP) with inline OSC 8 text markers.
2. OSC 8-capable telnet clients receive native terminal hyperlinks.
3. MXP telnet clients continue to receive `<send>` tags (existing behavior, unchanged).
4. Plain clients receive plain text (existing behavior, unchanged).
5. Zero changes to the 70+ existing `mxp_*` call sites.
6. Player-facing links (look, get, examine) for non-staff; admin links (stat, edit, purge)
   filtered server-side by trust level.
7. Unified `COMM_LINKS` player preference replaces `COMM_MXP`.

## Non-Goals

- Web client UI rendering of links (that's the Web Client plan).
- Mudlet Lua package for Sentience.Link (Phase 4).
- New link types beyond what `mxp_links.c` already generates.

---

## Architecture

### Link Mode Routing

A new `link_mode()` helper returns an enum indicating the best link delivery method:

```c
typedef enum {
    LINK_NONE,   /* Plain text, no links */
    LINK_MXP,    /* Telnet MXP <send> tags (legacy) */
    LINK_OSC8,   /* Telnet OSC 8 hyperlinks */
    LINK_GMCP    /* WebSocket GMCP + OSC 8 markers */
} link_mode_t;
```

Routing priority:

```
link_mode(descriptor_t *d)
  ├── WebSocket connection         → LINK_GMCP  (always, no player toggle needed)
  ├── Player COMM_LINKS off        → LINK_NONE
  ├── MXP negotiated               → LINK_MXP   (existing telnet clients)
  ├── TTYPE indicates OSC 8        → LINK_OSC8   (modern terminals)
  └── Otherwise                    → LINK_NONE
```

### Function Routing (inside each mxp_* function)

```
mxp_obj_link(d, buf, obj, text)
  ├── LINK_GMCP  → embed OSC 8 markers in buf + queue link entry on descriptor
  ├── LINK_OSC8  → embed OSC 8 hyperlink in buf (mud:// scheme, primary action only)
  ├── LINK_MXP   → emit <send> tags in buf (existing behavior, unchanged)
  └── LINK_NONE  → append plain text to buf
```

### Files

| File | Change | Purpose |
|------|--------|---------|
| `mxp_links.h` | Modify | Add `link_mode_t`, `link_mode()`, `sentience_link_entry_t`, queue types |
| `mxp_links.c` | Modify | Add 4-way routing to all 11 functions, link queue helpers, trust filtering |
| `protocol.h` | Modify | Add `sentience_link_queue_t` to `protocol_t` struct |
| `protocol.c` | Modify | Initialize/free link queue in `ProtocolCreate()`/`ProtocolDestroy()` |
| `comm.c` | Modify | Call link queue flush before output write; expose `descriptor_is_websocket()` |
| `tests/unit/link_routing_tests.c` | Create | Unit tests for link mode detection, JSON building, queue operations |
| `tests/data/unit/link_routing_unit_tests.json` | Create | Test case data |

---

## GMCP Protocol: Sentience.Link.List

Sent as a GMCP message just before the text buffer flushes.  The web client receives this
message, then the text (containing OSC 8 markers), and correlates by link ID.

### Message Format

```json
Sentience.Link.List [
  {
    "id": "lk_0",
    "text": "a steel sword",
    "hint": "A gleaming steel sword lies here",
    "actions": [
      { "label": "Look",  "cmd": "look sword",   "hint": "Examine the sword" },
      { "label": "Get",   "cmd": "get sword",    "hint": "Pick up the sword" }
    ],
    "category": "obj"
  },
  {
    "id": "lk_1",
    "text": "Gandalf",
    "hint": "A wise wizard stands here",
    "actions": [
      { "label": "Look",  "cmd": "look gandalf",  "hint": "Look at Gandalf" },
      { "label": "Follow", "cmd": "follow gandalf", "hint": "Follow Gandalf" }
    ],
    "category": "player"
  }
]
```

### Field Reference

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | yes | Link ID matching OSC 8 marker in text (`lk_0`, `lk_1`, ...) |
| `text` | string | yes | Display text (for client validation/fallback) |
| `hint` | string | no | Hover tooltip for the link |
| `actions` | array | yes | Ordered list of actions; first = primary/left-click |
| `actions[].label` | string | yes | Context menu item text |
| `actions[].cmd` | string | yes | MUD command to execute |
| `actions[].hint` | string | no | Context menu item tooltip |
| `category` | string | yes | Link type: `obj`, `mob`, `room`, `player`, `help`, `cmd` |

### Client Behavior

- **Left-click / tap**: execute first action's `cmd`
- **Right-click / long-press**: show context menu with all actions
- **Hover**: show top-level `hint` as tooltip
- **Context menu item hover**: show per-action `hint`

---

## Inline Markers: OSC 8 Escape Sequences

### Format

OSC 8 is the de-facto standard for terminal hyperlinks:

```
\x1b]8;;<uri>\x07<text>\x1b]8;;\x07
```

For GMCP-capable clients (WebSocket), the URI is a link ID reference:

```
\x1b]8;;lk_0\x07a steel sword\x1b]8;;\x07
```

The web client strips the OSC 8 escapes, extracts `lk_0`, looks it up in the most recent
`Sentience.Link.List` message, and renders a clickable element with context menu.

For OSC 8-capable telnet clients (no GMCP), the URI encodes the primary command:

```
\x1b]8;;mud://look%20sword\x07a steel sword\x1b]8;;\x07
```

Terminals that support OSC 8 render this as a clickable hyperlink. Those that don't
silently ignore the escape sequences (text still displays correctly).

### mud:// URI Scheme

For telnet OSC 8 links, we use a `mud://` scheme to encode the primary command:

```
mud://<url-encoded-command>
```

Examples:
- `mud://look%20sword` → executes `look sword`
- `mud://help%20fireball` → executes `help fireball`
- `mud://goto%203%3A1024` → executes `goto 3:1024`

Mudlet's OSC 8 handler can be configured to intercept `mud://` URIs and execute them as
MUD commands.  Other OSC 8 terminals will display the link but clicking it opens the URI
in a browser (harmless no-op for `mud://`).

**Limitation:** OSC 8 telnet links support only the primary action (single click).
Multi-action context menus require GMCP (WebSocket) or MXP (telnet).

---

## Link Accumulator

### Data Structures (on protocol_t)

```c
#define SENTIENCE_LINK_QUEUE_INITIAL 16

typedef struct {
    char id[16];           /* "lk_0", "lk_1", ... */
    char *text;            /* Display text (strdup'd) */
    char *hint;            /* Hover tooltip (strdup'd, may be NULL) */
    char *category;        /* "obj", "mob", "room", etc. */
    int num_actions;
    struct {
        char *label;       /* Context menu text */
        char *cmd;         /* MUD command */
        char *hint;        /* Action tooltip (may be NULL) */
    } actions[8];          /* Max 8 actions per link */
} sentience_link_entry_t;

typedef struct {
    int count;
    int capacity;
    sentience_link_entry_t *entries;
} sentience_link_queue_t;
```

### Lifecycle

1. **Init**: `ProtocolCreate()` — allocate queue with initial capacity of 16
2. **Accumulate**: Each `mxp_*` call in LINK_GMCP mode appends an entry via
   `sentience_link_queue_add()`
3. **Flush**: Before output buffer writes to socket, call `sentience_link_queue_flush(d)`:
   - If `queue->count > 0`: build JSON array, send `Sentience.Link.List` via `SendGMCPRaw()`
   - Reset `queue->count` to 0 (entries reused, strings freed)
4. **Free**: `ProtocolDestroy()` — free all entries and the queue itself

### Link ID Generation

Per-descriptor counter, reset on each flush:

```c
snprintf(entry->id, sizeof(entry->id), "lk_%d", queue->count);
```

IDs are compact (`lk_0` through `lk_N`) and only meaningful within the scope of one
output flush.  The web client replaces its link table on each `Sentience.Link.List`
message (no persistence across messages needed).

---

## Trust-Based Action Filtering

Each entity-specific `mxp_*` function checks `IS_IMMORTAL(d->character)` to decide
which actions to include:

| Function | Player Actions | Staff Actions |
|----------|---------------|---------------|
| `mxp_obj_link` | look | stat obj, oshow, oedit, purge |
| `mxp_obj_id_link` | (id display only) | stat obj, purge |
| `mxp_obj_vnum_link` | look | oshow, oedit |
| `mxp_mob_link` | look | stat mob, mshow, medit |
| `mxp_room_link` | look | rshow, redit, goto |
| `mxp_player_link` | look, tell | stat char |
| `mxp_help_link` | help | (none) |
| `mxp_command_link` | (caller-defined) | (caller-defined) |
| `mxp_link` | (caller-defined) | (caller-defined) |
| `mxp_link_multi` | (caller-defined) | (caller-defined) |
| `mxp_link_prompt` | (caller-defined) | (caller-defined) |

Generic functions (`mxp_link`, `mxp_link_multi`, `mxp_command_link`, `mxp_link_prompt`)
pass through whatever actions the caller provides — filtering is the caller's responsibility
for these.

**MXP backward compatibility:** The existing MXP path continues to emit all commands
(including admin) as it does today — MXP tags are harmless because commands are
permission-checked server-side.

---

## Player Preference: COMM_LINKS

### Migration

- Add `COMM_LINKS` as an alias at the same bit position as `COMM_MXP` (both names
  resolve to the same flag value).  Existing code using `COMM_MXP` continues to work.
- Players who had MXP enabled automatically have links enabled (same bit).
- `config links` toggles the preference (preferred name going forward).
- `config mxp` kept as deprecated alias, prints migration notice pointing to `config links`.
- WebSocket connections ignore this flag (links always enabled).

### TTYPE-Based OSC 8 Detection

A lookup table of known OSC 8-capable terminal strings:

```c
static const char *osc8_terminals[] = {
    "mudlet",          /* Mudlet 4.18+ */
    "xterm-256color",  /* xterm, many modern terminals */
    "xterm-kitty",     /* Kitty terminal */
    "tmux-256color",   /* tmux (passes through OSC 8) */
    NULL
};
```

`has_osc8_support(d)` checks `d->pProtocol->pLastTTYPE` (the TTYPE string received
during telnet negotiation) against this table using case-insensitive prefix matching.

---

## Testing

### Unit Tests

| Test | What It Verifies |
|------|-----------------|
| `link_mode_*` | Routing logic: WebSocket → GMCP, MXP → LINK_MXP, TTYPE → OSC8, plain → NONE |
| `link_queue_*` | Accumulate entries, flush builds correct JSON, reset clears state |
| `link_json_*` | Sentience.Link.List JSON structure: fields, types, action arrays |
| `link_osc8_*` | OSC 8 marker generation: correct escapes, ID embedding, mud:// URI encoding |
| `link_trust_*` | Trust filtering: player sees gameplay actions, staff sees all actions |
| `link_mxp_compat_*` | MXP path unchanged: same `<send>` tag output as before |

Test handler prefix: `slink_` (registered in test_dispatcher.c as `MATCH_SUBSTR`).

### Manual Verification

- Connect via Mudlet with MXP → verify existing `<send>` tags work
- Connect via Mudlet 4.18+ without MXP → verify OSC 8 hyperlinks appear
- Connect via web client → verify context menus appear on linked text
- Connect via plain telnet → verify no escape sequences in output

---

## Risks & Open Questions

1. **OSC 8 in MCCP stream**: OSC 8 escapes pass through MCCP compression unchanged,
   but verify with Mudlet that compressed OSC 8 renders correctly.
2. **TTYPE detection accuracy**: The terminal string table is a best-effort heuristic.
   Some terminals report generic strings. The player `config links` toggle is the fallback.
3. **Link queue memory**: Dynamic array grows with output complexity. Cap at 256 links
   per flush to prevent unbounded growth from pathological output.
4. **MXP tag stripping**: The protocol output layer currently strips MXP control characters
   (`\x12`, `\x13`) for non-MXP connections. Verify OSC 8 escapes (`\x1b]8`) are NOT
   stripped by this same path.
5. **Flush timing**: The link queue flush hooks into `process_output()` in `comm.c`
   (line ~3154), just before the `write_to_descriptor()` call that writes `d->outbuf`
   to the socket.  This ensures GMCP link data arrives before the annotated text.

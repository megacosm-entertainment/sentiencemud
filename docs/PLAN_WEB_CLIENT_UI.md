# PLAN: Web Client UI

**Status:** Design Draft
**Date:** 2026-03-24
**Related Plans:** `PLAN_GMCP_REWORK.md`, `PLAN_WEB_INFRASTRUCTURE.md`

---

## Overview

The web client is currently a basic terminal (xterm.js over WSS). This plan expands it into
a full GUI application with dockable panels — providing the same moveable/resizable
mini-window experience as Mudlet, but in the browser, with the added advantage that layout
preferences are saved server-side and roam across devices.

The terminal output pane remains the core. All other panels are optional overlays driven
by `Sentience.*` GMCP packages (see `PLAN_GMCP_REWORK.md`).

---

## Layout Engine

### Library: Golden Layout

**[Golden Layout](https://golden-layout.com/)** is the recommended panel system.

- Panels snap to a grid, can be tabbed, dragged, resized, or floated
- Serializes/deserializes its entire state as JSON natively
- MIT license, actively maintained
- Used by Bloomberg Terminal web UI and VSCode-inspired applications
- Framework-agnostic (works with React, Vue, or vanilla JS)

Alternative considered: **React Mosaic** — simpler tiling but no floating/tabbing.
Alternative considered: **Floating UI** — lower-level, would require building the drag
system from scratch. Not recommended.

### Terminal Pane Constraints

The terminal panel is special:
- **Always present** — cannot be closed or removed from the layout
- **Cannot float** (floating it would remove it from the main layout)
- **Minimum size enforced** — e.g., no smaller than 400×300px
- All other panels are optional and closeable

Golden Layout supports `isClosable: false` and minimum size constraints per component.

---

## Panel Definitions

### Default Layout

```
┌──────────────────────────┬──────────────┐
│                          │  Vitals      │
│                          │  (HP/MP/MV)  │
│     Terminal             ├──────────────┤
│     (main output)        │  Room Info   │
│                          │  (+ exits)   │
│                          ├──────────────┤
│                          │  Minimap     │
├──────────────────────────┴──────────────┤
│  Input bar                              │
└─────────────────────────────────────────┘
```

Players can drag, resize, tab, or float any non-terminal panel.

### Panel Inventory

| Panel | GMCP Source | Default position | Closeable |
|-------|------------|-----------------|-----------|
| Terminal | Raw text stream | Center, fills remaining space | No |
| Vitals | `Sentience.Char.Vitals` | Top-right | Yes |
| Stats | `Sentience.Char.Stats` | Tabbed with Vitals | Yes |
| Room Info | `Sentience.Room.Info` | Right sidebar | Yes |
| Minimap | `Sentience.Room.Info` exits | Right sidebar, tabbed | Yes |
| Combat | `Sentience.Char.Enemies` | Bottom or float | Yes |
| Effects/Buffs | `Sentience.Char.Affects` | Right or bottom | Yes |
| Inventory | `Sentience.Char.Inventory` (Phase 3) | Left or float | Yes |
| Character Sheet | `Sentience.Char.Identity` + Stats | Float | Yes |
| Quest Tracker | `Sentience.Quest.Active` (Phase 3) | Float or tabbed | Yes |
| Chat | `Sentience.Channel.*` GMCP | Bottom or tabbed with terminal | Yes |

---

## Panel Designs

### Vitals Panel

Displays HP/mana/move as labeled progress bars with current/max values.

```
HP   ████████░░░░  85 / 120
MP   ██████████░░  110 / 130
MV   ████░░░░░░░░   40 / 100
```

Color thresholds (configurable):
- Green: > 75%
- Yellow: 25–75%
- Red: < 25%

Updates on `Sentience.Char.Vitals` dirty event only (not every pulse).

### Stats Panel

Two-column grid: current stat / base stat. Shows combat modifiers (hitroll, damroll) below.

```
STR  18 (18)    INT  12 (12)
WIS  14 (14)    DEX  16 (16)
CON  15 (15)    

Hit  +5    Dam  +3    AC  -80
```

### Room Info Panel

Room name, area name, and a grid of exit buttons. Exits with doors shown differently from
open exits. Unknown exits greyed out.

```
The Crossroads
Midgaard (Midgaard City)

[N] [NE] [E]
[NW]  ·  [SE]
[W]  [SW] [S]

[U] [D]
```

Clicking an exit button sends the direction command. Exit buttons are generated from
`Sentience.Room.Info.exits`.

### Minimap Panel

The game already generates an ASCII minimap (in `act_info.c` — `show_map_to_char` for
wilderness, `show_map_and_description` for area rooms) displayed inline in the terminal on
room entry. For the web client panel, this existing output can be redirected into the panel
instead of printing inline in the terminal.

**Approach:** When the minimap panel is enabled, the server sends the pre-rendered ANSI map
as a `Sentience.Room.Map` GMCP package instead of inline terminal output:

```
Sentience.Room.Map
  map_text    string   Pre-rendered ANSI map (existing show_map_to_char output)
  type        string   "wilds" | "area" | "dungeon"
  width       int      Character columns
  height      int      Character rows
```

The web client renders `map_text` in a small xterm.js sub-instance (or ANSI-to-HTML
renderer) inside the panel. This reuses 100% of existing map generation code with no
changes to rendering logic.

**Suppression in terminal:** When the minimap panel is active, suppress the inline
`show_map_and_description` / `show_map_to_char` call on room entry so the map doesn't
appear twice. Suppression is per-descriptor, keyed off a panel capability flag sent by the
client in `Sentience.Client.Ready`.

This is the same pattern used for Vitals/Stats panels — the server already has the data,
the panel just intercepts it before it hits the terminal stream.

### Combat Panel

Only visible when `Sentience.Char.Enemies` is non-empty. Shows each enemy with a health bar.

```
Goblin Warrior    ████████░░  (fighting you)
Goblin Shaman     ██████████  (fleeing)
```

Clicking an enemy name sends `kill <name>`. Auto-hides when combat ends.

### Effects/Buffs Panel

Icon grid or list of active effects from `Sentience.Char.Affects`. Each effect shows name
and duration (pulses remaining). Hovering shows full description (requires `Sentience.Char.Affects`
to include a description field — add to the GMCP plan).

### Chat Panel

A tabbed panel that intercepts channel output from the text stream and displays it in
per-channel tabs. This lets players separate noisy channels (gossip, OOC, trade) from
the main terminal output without missing messages.

**Tab model:**
```
[ All ] [ Gossip ] [ OOC ] [ Trade ] [ Tells ] [ Org ]
──────────────────────────────────────────────
[12:03] Aeryn: Anyone selling a sword?
[12:04] Brek: I have a few. Check the market.
```

**Tabs:**
- **All** — always present, shows every channel message (cannot be removed)
- Per-channel tabs — one per channel the player is subscribed to, user-configurable
- **Tells** — private messages consolidated into one tab
- **System** — optional tab for server messages, deaths, area events

**Server side — new GMCP package:**
```
Sentience.Channel.Message
  channel     string   Channel name/identifier (matches channel topic)
  sender      string   Character name (or "" for system messages)
  text        string   Message content (ANSI color stripped — web renders its own)
  timestamp   int      Unix timestamp
  tell_target string   (tells only) recipient name
```

**Suppression in terminal:** When the chat panel is active and a channel tab exists for
a given channel, the server suppresses that channel's output from the main text stream
(similar to map suppression). The `All` tab always receives everything regardless.
Players can configure per-channel whether to suppress from terminal or show in both.

**Input:** Each tab has a channel-specific input shortcut. Clicking a tab's input
area prefills the command prefix (e.g., typing in the Gossip tab prepends `gossip `).

**Unread indicators:** Tabs with new messages since last viewed show a dot or count badge.

**Persistence:** Which channels have tabs, and the suppress-from-terminal setting per
channel, are saved in the layout state (part of `Sentience.Client.Layout`).

### Input Bar

Always visible at bottom. Single-line input with command history (↑/↓). Distinct from the
terminal so it's always accessible regardless of terminal scroll position.

Command history: 50 items, client-side session storage (not persisted server-side — matches
Mudlet behavior).

---

## Layout Persistence

### Mechanism

Golden Layout serializes its state to a plain JSON object. This is saved server-side via
a `Sentience.Client.Layout` GMCP message:

```json
// Client → Server (save)
{
  "action": "save",
  "layout": { /* Golden Layout state JSON */ }
}

// Server → Client (restore on login)
{
  "action": "restore",
  "layout": { /* saved Golden Layout state JSON */ }
}
```

Server stores this in the character JSON under `"web_client_layout"`. Maximum size: 8KB
(prevents abuse; Golden Layout state is typically <2KB).

### Fallback

If no saved layout exists (new player or first web login), load the default layout defined
in the client bundle. Players can always reset to default via a menu option.

### Save Trigger

Save on:
- Panel drag/resize end
- Panel open/close
- Tab reorder

Debounced: minimum 2 seconds between saves to avoid rapid-fire GMCP messages during
active layout manipulation.

---

## Context Menus (Sentience.Link)

When the server sends a `Sentience.Link` GMCP packet alongside annotated text:
- Left-click on the linked text → execute `left` command
- Right-click → show custom context menu with `right[]` items
- Long-press (mobile/touch) → same as right-click

The context menu is a styled overlay positioned at cursor/touch location. It dismisses on
click-outside or Escape.

```
┌─────────────────┐
│ Examine sword   │
│ Pick up sword   │
│ Drop sword      │
│ Wield sword     │
└─────────────────┘
```

Menu items from `Sentience.Link.right[].label` / `.cmd`.

---

## Mobile / Touch Support

All panels must work on mobile:
- Panels stack vertically in a single column on narrow screens (< 768px)
- Long-press replaces right-click for context menus
- Exit buttons in Room Info panel are touch-friendly (minimum 44×44px tap target)
- Vitals panel always visible (pinned to top on mobile)
- Floating panels disabled on mobile (no drag precision)

Golden Layout has a responsive mode; the mobile layout is a separate simplified config
that activates below the breakpoint.

---

## Technology Stack

- **Golden Layout** — panel layout engine
- **xterm.js** — terminal emulator (existing, unchanged)
- **React** — component framework (aligns with Next.js / Payload CMS stack in `PLAN_WEB_INFRASTRUCTURE.md`)
- **CSS custom properties** — theming (dark/light mode, color scheme matching in-game colors)

No additional backend dependencies — layout state goes over the existing GMCP WebSocket
connection.

---

## Theming

Players can choose a color theme. The theme applies to panels; the terminal uses its own
ANSI color mapping independently.

Suggested defaults:
- **Dark** (default) — dark charcoal panels, matching terminal dark background
- **Light** — light panels for accessibility
- **Immersive** — minimal chrome, panels semi-transparent over a parchment background

Theme preference stored in character JSON alongside layout state.

---

## Implementation Phases

### Phase 1 — Foundation panels
- [ ] Integrate Golden Layout into web client
- [ ] Default layout: terminal (center) + vitals (right) + room info (right, tabbed)
- [ ] Vitals panel with HP/mana/move bars, driven by `Sentience.Char.Vitals`
- [ ] Room Info panel with exit buttons, driven by `Sentience.Room.Info`
- [ ] Input bar (always visible, command history)
- [ ] Terminal panel constraints (non-closeable, minimum size)

### Phase 2 — Persistence and more panels
- [ ] `Sentience.Client.Layout` GMCP message (server receive + send on login)
- [ ] Character JSON storage for layout state
- [ ] Stats panel
- [ ] Combat panel (auto-show/hide)
- [ ] Effects/buffs panel
- [ ] Context menu handler for `Sentience.Link`
- [ ] Chat panel: `Sentience.Channel.Message` GMCP package (server side)
- [ ] Chat panel: tabbed UI with All/per-channel tabs, unread indicators
- [ ] Chat panel: per-channel terminal suppression

### Phase 3 — Rich panels
- [ ] Minimap (client-side graph from navigation history)
- [ ] Inventory panel (requires `Sentience.Char.Inventory` GMCP package)
- [ ] Character sheet panel
- [ ] Quest tracker panel (requires `Sentience.Quest.Active` GMCP package)
- [ ] Mobile layout

### Phase 4 — Polish
- [ ] Theming system (dark/light/immersive)
- [ ] Touch/long-press context menus
- [ ] Panel reset-to-default option
- [ ] Accessibility review (keyboard navigation, ARIA labels on panels)

---

## Open Questions

1. **Golden Layout v1 vs v2**: v2 is a full rewrite (still in beta as of early 2026). v1 is
   stable but older. Worth evaluating v2 beta stability before committing.

2. **Minimap depth**: How many rooms should the client cache for the minimap? 50? 200?
   This affects memory and the usefulness of the map on long exploration sessions. The map
   resets on area change (room wnum area prefix changes).

3. **Wilderness minimap**: Wilderness rooms have x/y coordinates — display as a tile grid
   centered on current position. Grid size TBD (e.g., 11×11 tiles visible).

4. **Panel descriptions for Affects**: Should `Sentience.Char.Affects` include a
   human-readable description per effect, or just name + duration? Descriptions would
   enable tooltip/hover in the Effects panel but add GMCP payload size.

5. **Input bar history persistence**: Keep command history server-side (persisted in
   character JSON) so it survives refresh/reconnect, or session-only (simpler)?

6. **Layout versioning**: If the panel structure changes in a future version (e.g., a panel
   is renamed or removed), how do we handle stale saved layouts? Probably: on load, validate
   each panel component type against a known list; drop unknown components gracefully.

7. **Panel plugin model**: Should third-party panels be possible (e.g., a guild-specific
   panel defined by a builder)? Out of scope for now but worth not designing against.

---

## See Also

- `PLAN_GMCP_REWORK.md` — `Sentience.*` GMCP packages that feed panel data
- `PLAN_WEB_INFRASTRUCTURE.md` — overall web stack (Next.js, Payload CMS, WSS)
- `PLAN_CLIENT_AUTOMATION.md` — server-side automation (triggers, timers, aliases)

# MXP Links and bprintf

## Overview

This document covers two related improvements to output formatting:

1. **`bprintf`** — a safe `printf`-style append for `BUFFER`, replacing the `sprintf(buf, ...); add_buf(buffer, buf);` pattern.
2. **`mxp_links`** — standardized helpers for building MXP `<send>` tags with clickable commands and tooltips.

## bprintf

### Problem

The codebase uses a two-step pattern everywhere:

```c
char buf[MSL];
sprintf(buf, "Level: %d, Name: %s\n\r", level, name);
add_buf(buffer, buf);
```

This has two issues:
- `sprintf` into a fixed stack buffer (`buf[MSL]`) can overflow silently.
- The intermediate copy is wasteful — the data goes stack → BUFFER when it could go directly.

### Solution

`bprintf` formats directly into the BUFFER with no intermediate copy:

```c
bprintf(buffer, "Level: %d, Name: %s\n\r", level, name);
```

It uses `vsnprintf(NULL, 0)` to measure the needed size, grows the BUFFER if necessary, then `vsnprintf` directly at the append point. No fixed-size intermediary, no overflow risk.

### Declaration

```c
// recycle.h
bool bprintf(BUFFER *buffer, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
```

The `format` attribute enables compiler warnings for format string mismatches.

### Migration

Replace all instances of:
```c
char buf[MSL];  // or MIL, MIL*2, etc.
sprintf(buf, "...", args);
add_buf(buffer, buf);
```

With:
```c
bprintf(buffer, "...", args);
```

The `char buf[]` local can be removed once all uses in a function are converted.

## MXP Link Helpers

### Problem

MXP `<send>` tags were built manually with massive `sprintf` calls:

```c
sprintf(buf, "\t<send href=\"stat obj %ld %ld|||purge obj %ld %ld\" "
    "hint=\"Show information||***DANGER***|Purge\">{W%ld %ld{X\t</send>",
    id[0], id[1], id[0], id[1], id[0], id[1]);
```

This is error-prone, unreadable, and the command/hint pipe-alignment is fragile.

### Solution

Domain-specific helpers in `mxp_links.h` / `mxp_links.c` that append directly to a BUFFER:

```c
// Object ID with stat/purge actions
mxp_obj_id_link(ch->desc, buffer, obj);

// Object name with show/edit actions
mxp_obj_vnum_link(ch->desc, buffer, obj->pIndexData, obj->short_descr);

// Mobile with stat/show/edit (auto-detects NPC vs player)
mxp_mob_link(ch->desc, buffer, mob, mob->short_descr);

// Room with show/edit/goto
mxp_room_link(ch->desc, buffer, room, "Room 1#500");

// Player with stat
mxp_player_link(ch->desc, buffer, name, name);

// Help topic
mxp_help_link(ch->desc, buffer, "score", "score");

// Generic single command
mxp_command_link(ch->desc, buffer, "look", "Look around", "look");
```

All helpers gracefully degrade — when MXP is not enabled, they append the plain display text.

### Low-Level API

For non-standard link patterns, use `mxp_link` or `mxp_link_multi`:

```c
// Single command with hint
mxp_link(ch->desc, buffer, "displayed text", "command", "tooltip");

// Multiple commands (shown as a menu in MXP clients)
mxp_cmd_hint_t items[] = {
    { "stat obj 1 2",  "Stat object" },
    { "purge obj 1 2", "Purge object" },
};
mxp_link_multi(ch->desc, buffer, "displayed text", items, 2);
```

### Available Domain Helpers

| Function | Commands Generated | Use Case |
|---|---|---|
| `mxp_obj_link` | stat obj / oshow / oedit / purge obj | Object instance (full actions) |
| `mxp_obj_id_link` | stat obj / purge obj | Object ID display |
| `mxp_obj_vnum_link` | oshow / oedit | Object index link |
| `mxp_mob_link` | stat mob / mshow / medit | NPC instance |
| `mxp_room_link` | rshow / redit / goto | Room link |
| `mxp_player_link` | stat char | Player link |
| `mxp_help_link` | help | Help topic |
| `mxp_command_link` | (any single command) | Generic command |

### Example: Before and After

**Before (do_owhere):**
```c
sprintf(buf, "{Y%3d) {WID{X: [\t<send href=\"stat obj %ld %ld|||purge obj %ld %ld\" "
    "hint=\"Show information for this object||***DANGER***|Purge this object\">"
    "{W%ld %ld{X\t</send>]{x \t<send href=\"oshow %s|oedit %s\" "
    "hint=\"Show index for %s|Edit %s\">%s\t</send> is carried by "
    "\t<send href=\"stat mob %ld %ld|mshow %s|medit %s\" "
    "hint=\"View info for %s|Show index for %s|Edit %s\">%s\t</send> "
    "[\t<send href=\"rshow %s|redit %s|goto %s\" "
    "hint=\"View room %s|Edit room %s|Go to room %s\">Room %s\t</send>]\n\r",
    /* ... 20+ arguments with repeated ternary expressions ... */);
add_buf(buffer, buf);
```

**After:**
```c
bprintf(buffer, "{Y%3d) {WID{X: [", number);
mxp_obj_id_link(ch->desc, buffer, obj);
bprintf(buffer, "]{x ");
mxp_obj_vnum_link(ch->desc, buffer, obj->pIndexData, obj->short_descr);
bprintf(buffer, " is carried by ");
mxp_mob_link(ch->desc, buffer, carrier, carrier->short_descr);
bprintf(buffer, " [");
mxp_room_link(ch->desc, buffer, carrier->in_room,
    formatf("Room %s", widevnum_string_room(carrier->in_room, NULL)));
bprintf(buffer, "]\n\r");
```

## Files

- `src/mem.c` — `bprintf` implementation
- `src/recycle.h` — `bprintf` declaration
- `src/mxp_links.h` — MXP helper declarations
- `src/mxp_links.c` — MXP helper implementations

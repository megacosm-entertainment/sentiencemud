# Builder Changelog (OLC / Content)

Scope: `df6c9d2c3c0c1e5c93172ce2da08868a2c9aa0e4` → `88602021f775e53d514521366df2948fdffb37ea` (current HEAD)

This channel is for content builders using OLC and data-backed world systems.
It focuses on content-authoring additions, content-side bug fixes, and removed builder paths.

Major context: this is a migration-era builder surface with new editor architecture, widevnum transition work, expanded event/quest tooling, and broad race/class/skill/wilderness data shifts.

## Additions

### Modular OLC Framework
- Editor code was split into dedicated modules under `src/editors/` with a shared framework.
- Major editors now include dedicated modules for areas, rooms, objects, mobs, scripts, socials, events, skills, dungeons, wilderness, classes, sectors, channels, reputation, traits, and more.
- Common editor behavior is centralized (`editors/common.c`, `editors/common/*`) for better consistency.

### New/Expanded Builder Editors
- `socialedit` for social definitions.
- `evtedit` for event definitions and schedule/progress/phase policy.
- `rsgedit` for random-string generator content.
- `reserved` editor for symbolic VNUM mapping.
- `cedit` for channel definitions.

### Data-Driven Content Backends
- Core gameplay content moved to JSON/data-driven forms (skills, classes, races, traits, sectors, socials, commands, reserves).
- Bootstrap seed content was added to `src/bootstrap/bootstrap_data/*` to keep fresh installs aligned with authored content.

### Widevnum and Cross-Area Safety
- Continued migration away from brittle single-vnum assumptions toward widevnum-aware addressing and resolution.
- Reserved mappings and load-resolution helpers improve safer cross-area references.

### Quest and Wilderness Authoring Evolution
- Quest/runtime systems received substantial updates, including data/runtime integration changes.
- Wilderness received major expansion (state/storage/mods/vlinks/wmap/wterr), changing how builders reason about dynamic world state.

### Event Runtime Authoring Surface
- Event definitions now support schedule modes, bracket definitions, progress aggregation, phase plans, reward scripts, and roster metadata.
- Script/runtime integration for event provenance and progress control was expanded.

## Bug Fixes

### OLC Reliability and Consistency
- Editor behavior and save flows were normalized through common framework helpers.
- Multiple OLC/runtime validation hardening changes landed (notably around events and data constraints).

### Data Integrity in Content Systems
- JSON loader/saver coverage was expanded across many content domains to reduce format drift and partial-save issues.
- Cross-system references (e.g., reserved IDs/widevnums) gained better support in runtime/editor paths.

## Removals

- Legacy monolithic OLC flows were reduced in favor of modular editors.
- Legacy standalone OLC files were removed/replaced (`olc_edit_rsg.c`, `olc_mpcode.c` removed in favor of editor modules/runtime wiring).
- `social.c` legacy path removed in favor of data/editor-backed social handling.

## Builder Workflow Examples

### Create a social
```text
socialedit create salute
name salute
summary A crisp formal salute.
save
```

### Create a basic event definition
```text
evtedit create dragonhunt
type collection
scope global
schedule manual
goal 100
enabled on
save
```

### Create a reserved VNUM alias
```text
reserved create room ROOM_VNUM_TEMPLE 3001
reserved save
```

### Build a random name generator
```text
rsgedit create fantasy_names
class create prefix
class add prefix 50 Al
pattern create 100 {prefix}dar
generate 5
save
```

## Further Reading

- [Event Editor Runtime](EVENT_EDITOR_RUNTIME.md)
- [Event System Admin Guide](EVENT_SYSTEM_ADMIN_GUIDE.md)
- [RSGEdit Usage Guide](RSGEDIT_USAGE.md)
- [Skill/Spell/Command Guide](SKILL_SPELL_COMMAND_GUIDE.md)
- [OLC Refactor Plan](PLAN_OLC_REFACTOR.md)
- [Widevnum Implementation Plan](widevnums/WIDEVNUM_IMPLEMENTATION_PLAN.md)

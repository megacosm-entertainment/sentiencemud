# Player Changelog

Scope: `df6c9d2c3c0c1e5c93172ce2da08868a2c9aa0e4` → `88602021f775e53d514521366df2948fdffb37ea` (current HEAD)

This is the player-facing summary of the large migration window.
It focuses on gameplay-visible behavior, quality of life, and removed player systems.

Major context: this range introduced new account foundations, identity systems (pronouns/body type), large persistence/storage upgrades, the new event framework, and broad world/system modernization.

## Additions

### Identity and Character Presentation
- Pronouns are fully customizable and no longer tied to body type.
- Pronoun grammar supports all five forms plus verb preference.
- Character setup/login flows were expanded to support these identity options cleanly.

### Better Account and Session Experience
- Account/session handling was reworked for more reliable login, reconnect, and character selection.
- Account preferences and related account management systems were expanded.
- Shared storage and account-linked system behavior were expanded as part of the account overhaul.

### Event Participation Surface
- Player/staff event visibility is improved through `event list`, `event info`, and `event status`.
- Event runtime tracking now supports richer progress and bracketed participation models.

### World and Progression Modernization
- Quest, wilderness, and related progression systems were heavily reworked under the hood.
- Widevnum migration work improves cross-area consistency and reduces entity-resolution edge cases.

### Social/Communication Foundation Upgrades
- Social and channel systems moved to data-driven backends, enabling cleaner behavior and easier iteration.
- Chat/history/filter infrastructure was significantly expanded for more consistent communication behavior.

## Bug Fixes

### Connection and Stability
- Fixed multiple TLS/SSL reliability issues, including SIGPIPE-related disconnect/crash paths.
- Fixed several crash paths in runtime update and caching flows.

### Reconnect and Persistence Safety
- Fixed reconnect edge-cases around heavy inventories and state resumption.
- Fixed account/character load duplication paths that could cause corruption/duplication symptoms.

### Character Data Correctness
- Fixed pronoun/body-type persistence edge cases.
- Fixed deleted-character/account handling issues during load/reconnect workflows.

## Removals

- IMC2 inter-mud communication was removed (`imc.c`, `imc.h`, `imccfg.h`).
- Legacy healer NPC subsystem was removed (`healer.c`).
- Legacy locker subsystem was removed (`locker.c`) in favor of newer account/storage direction.

## Player Examples

### Pronouns
```text
pronouns
pronouns show they them their theirs themselves
```

### Event visibility
```text
event list
event info dragonhunt
event status
```

### Channel history/reporting workflow (where enabled)
```text
history gossip
history gossip info 3
history gossip report <message-id> harassment language
```

## Further Reading

- [Channel Player Guide](channels/CHANNELS_PLAYER_GUIDE.md)
- [Event System Admin Guide](EVENT_SYSTEM_ADMIN_GUIDE.md) (contains shared `event` command examples)
- [JSON Character Fixes](done/JSON_CHARACTER_FIXES.md)
- [Object Duplication Master Fix](done/OBJECT_DUPLICATION_MASTER_FIX.md)

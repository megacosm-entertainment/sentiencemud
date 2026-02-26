# Administrator Changelog (Operations / Systems)

Scope: `df6c9d2c3c0c1e5c93172ce2da08868a2c9aa0e4` → `88602021f775e53d514521366df2948fdffb37ea` (current HEAD)

This channel is for administrators managing live systems, moderation, policy, security, and runtime operations (non-direct-content work).

Major context: this range includes account/auth modernization, storage/persistence migration, widevnum transition work, event/quest runtime expansion, and major wilderness/system refactors.

Compatibility note: these changes were rolled out with a compatibility-first approach, preserving live operational behavior where feasible and using staged migrations over abrupt cutovers.

## Additions

### Channel Operations and Moderation Stack
- New channel subsystem modules were added under `src/channels/` for policy, filtering, moderation, registry, review, and transports.
- Moderation tools include warning/mute/ban flows plus review queue integration.
- Channel transport now supports local and Redis-backed backends.

### Event Operations Surface
- Live event operations are expanded via runtime `event` commands and richer scheduling/progress controls.
- Definition/runtime split improves operational safety (`evtedit` for templates, `event` for active runs).

### Logging and Crash Diagnostics
- Unified logging layer (`log.c`, `log.h`) added with categorized logging and better diagnostics.
- Crash handling/trace support was introduced for production troubleshooting.

### Secrets and Config Management
- Centralized secret retrieval (`secret.c`, `secret.h`) with support for mounted secrets and environment fallback.
- Game/system config persistence expanded through JSON-backed system files.

### Account and Storage Migration Impact
- Account-linked behavior and persistence boundaries were reworked across load/save/auth flows.
- Storage and state integrity improvements reduced duplication/corruption risk in live operations.
- Migration behavior was designed to preserve continuity for existing data and operating workflows during transition.

### Widevnum and Runtime Resolution Hardening
- Widevnum/backport work improved cross-area entity resolution behavior in both runtime and tooling paths.
- Related tests and helper coverage expanded to reduce regression risk.
- Migration strategy favors compatibility bridges so legacy assumptions fail less catastrophically while content is modernized.

### Bootstrap and Environment Control
- Bootstrap subsystem added for reproducible data/system initialization.
- Build/run scripts (`build`, `config`, `compile`, `install`) standardized operational workflow.

## Bug Fixes

### Runtime Safety and Duplication/Corruption Paths
- Account loading duplication/corruption edge-cases were addressed.
- Object duplication and ghost-object persistence issues were addressed.
- Reconnect and persistence safety improved in account/character flows.

### Operational Stability
- TLS/SSL stability fixes reduced connection-level faults and crashes.
- Cache/runtime safety hardening improved reliability under load.
- Quest/event/wilderness runtime stability was improved alongside systemic refactors.

## Removals

- IMC2 subsystem removed (`imc.c`, `imc.h`, `imccfg.h`).
- Legacy locker subsystem removed (`locker.c`).
- Legacy message queue implementation removed (`msgqueue.c`).
- Legacy HTML path removed (`html.c`).

## Admin Workflow Examples

### Channel moderation
```text
chanwarn troublemaker gossip language
chanmute troublemaker gossip 30 repeated abuse
chanban spammer * advertisement spam
chanunmute troublemaker gossip
chanpenalties troublemaker
```

### Channel review queue
```text
rview list 20
rview read 3
rview ack 3 handled by moderator team
```

### Event operations
```text
event status
event start dragonhunt
event info dragonhunt
event complete dragonhunt objective complete
```

## Further Reading

- [Channels Admin Guide](channels/CHANNELS_ADMIN_GUIDE.md)
- [Event System Admin Guide](EVENT_SYSTEM_ADMIN_GUIDE.md)
- [Logging Schema](LOGGING_SCHEMA.md)
- [Bootstrap Guide](BOOTSTRAP.md)
- [AcctLink Duplication Bug](done/ACCTLINK_DUPLICATION_BUG.md)
- [Object Duplication Master Fix](done/OBJECT_DUPLICATION_MASTER_FIX.md)

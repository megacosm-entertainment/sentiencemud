# Changelog Overview (Major Migration Window)

Scope: `df6c9d2c3c0c1e5c93172ce2da08868a2c9aa0e4` → `88602021f775e53d514521366df2948fdffb37ea`

This release window is not a normal patch cycle. It is a platform-scale migration with major new systems and data-model transitions.

## Compatibility Posture

This migration series was executed with strong backward-compatibility goals:

- New systems were generally introduced alongside compatibility bridges rather than hard cutovers.
- Existing gameplay/operator flows were preserved where practical while internals were modernized.
- Migration work prioritized continuity of live data and runtime behavior.
- Legacy paths were only removed when replacement systems were already established.

## Biggest New Systems

- New account/auth foundation (account linking, auth centralization, preferences, penalties, unlock flow).
- New identity model (pronouns + body type separation and supporting creation/login flow updates).
- New storage/persistence direction (JSON-first save/load coverage across many gameplay and system domains).
- Widevnum migration work replacing hardcoded/single-vnum assumptions with safer cross-area addressing.
- Races/classes/skills/songs/traits moved toward structured data modules and bootstrap-seeded data.
- Event system introduced as a full definition/runtime model (`evtedit` + `event`) with scheduling, phases, brackets, and script integration.
- Quest/runtime progression surface expanded with supporting backend changes.
- Wilderness subsystem expanded with explicit state/storage/mod/link/map/terrain modules.
- Channel system elevated to a service architecture with moderation/filter/review/policy and transport layers.

## Removals and Legacy Retirements

- IMC2 removed.
- Legacy healer subsystem removed.
- Legacy locker subsystem removed.
- Multiple legacy/monolithic editor/runtime files removed or replaced by modular systems.

## Why This Matters

- Builders get more consistent OLC surfaces and data-backed workflows.
- Admins get stronger moderation, runtime visibility, and operational controls.
- Developers get a clearer module split, better test scaffolding, and lower risk in future migrations.
- Players get better stability, identity support, and system consistency.

## Backward-Compatible Migration Patterns Used

- Command/editor continuity: legacy command expectations were preserved while editor backends were modularized.
- Data-format continuity: JSON and bootstrap expansions were introduced to coexist with staged migration of older data assumptions.
- Runtime continuity: event/channel/wilderness/quest refactors were integrated incrementally with compatibility helpers and test coverage.
- Identifier continuity: widevnum migration work focused on reducing brittle assumptions without requiring abrupt content invalidation.

## Operator Notes

- Treat this range as a compatibility-first modernization, not a flag-day rewrite.
- Existing content should generally continue to function while newer systems are adopted over time.
- Validate local customizations against the new docs and test framework before removing any remaining legacy glue.

## Read by Audience

- [Player Changelog](CHANGELOG_PLAYERS.md)
- [Builder Changelog](CHANGELOG_BUILDERS.md)
- [Administrator Changelog](CHANGELOG_ADMINISTRATORS.md)
- [Developer Changelog](CHANGELOG_DEVELOPERS.md)

## Deep-Dive References

- [Event System Admin Guide](EVENT_SYSTEM_ADMIN_GUIDE.md)
- [Event Editor Runtime](EVENT_EDITOR_RUNTIME.md)
- [Channels Admin Guide](channels/CHANNELS_ADMIN_GUIDE.md)
- [Build System](BUILD_SYSTEM.md)
- [Integration Test Framework](INTEGRATION_TEST_FRAMEWORK.md)
- [Widevnum Implementation Plan](widevnums/WIDEVNUM_IMPLEMENTATION_PLAN.md)
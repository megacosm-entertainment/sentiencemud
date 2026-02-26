# PLAN: Dynamic Channels (Dungeon / Instance / Blueprint)

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


This plan refines Stage 7 from TODO_PUBSUB_MVP into implementation-oriented steps.

## Objectives

1. Dynamic channels are template-driven from `channels.json` definitions.
2. Access is strictly context-bound (e.g., only while in a dungeon/instance).
3. Runtime subscriptions are efficient and bounded (with grace TTL).
4. History hydration is correct across movement/membership transitions.

## Existing groundwork

- `channel_defs` attachment lists exist on:
  - `DUNGEON_INDEX_DATA`
  - `BLUEPRINT`
- JSON area load/save now round-trips those lists.
- Channel service already supports `INSTANCE_ID`/`DUNGEON_ID` scope topic builders.

## Open decisions to lock

1. Topic key shape for dynamic channel isolation:
   - Proposed: `rt:dungeon:<uid1>:<uid2>:<channel_id>` and `rt:instance:<uid1>:<uid2>:<channel_id>`
2. Should dynamic channel IDs be globally unique aliases or context + template ID tuple?
   - Proposed: tuple (`context`,`template_id`) with canonical generated topic.
3. History privacy after leaving context:
   - Proposed: deny new fetches outside context; allow already-hydrated local replay only.

## Implementation phases

### Phase A: Effective dynamic channel resolver

Add resolver in channel service:
- Input: actor context (`room`, `instance`, `dungeon`, `group`, `church`), attached `channel_defs`.
- Output: effective channel subscriptions and publish targets.

Key behavior:
- Resolve attachments from current dungeon index and instance blueprint.
- Validate channel IDs against registry.
- Produce deterministic topic strings including context and channel ID.

### Phase B: Access control gates

Add helper checks:
- `channel_actor_in_dungeon_context(actor, dungeon_uid)`
- `channel_actor_in_instance_context(actor, instance_uid)`

Enforce on both:
- publish path (sender)
- delivery path (recipient)

Rule:
- Context mismatch => drop/deny, regardless of subscription cache state.

### Phase C: Subscription lifecycle integration

Hook dynamic effective topics into `channel_service_sync_subscriptions`:
- Maintain refcount map with grace expiration timestamps.
- Resubscribe cancels pending expiration.
- Expiration unsubs when `now >= expires_at` and refcount still zero.

### Phase D: History hydration

On topic delta (`added` / `removed`):
- fetch recent history for `added` topics
- optional trailing fetch for `removed` topics disabled by default

Tell history remains separate participant-pair index (Stage 6.4).

### Phase E: Editor support

Add editor commands:
- `dedit channel add <id>`
- `dedit channel del <id>`
- `dedit channel list`
- `bpedit channel add <id>`
- `bpedit channel del <id>`
- `bpedit channel list`

Validation:
- Reject unknown channel IDs.
- De-duplicate entries.

## Testing matrix

1. Dungeon entry/exit access
2. Parallel dungeon instances isolation
3. Instance channels from blueprint attachments
4. Membership delta correctness while in dynamic contexts
5. JSON round-trip for attachment lists
6. Subscription grace behavior under rapid movement

## Rollout strategy

1. Ship resolver + guards disabled behind config flag (`dynamic_channel_defs_enabled=false`).
2. Enable in test/staging with verbose subscription logs.
3. Run stress test with movement and group churn.
4. Enable in production with conservative TTLs (10m room, 30m group/church/direct).

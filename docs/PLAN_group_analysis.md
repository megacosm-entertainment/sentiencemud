# PLAN: Group and Party System Rework

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


This document updates the grouping analysis to match current `src` reality and defines the next-phase party UX: invite/request flows with non-intrusive prompt markers (ready-check style) instead of interruptive dialogs.

## 1. Current State (as of `feature/widevnum-migration`)

The codebase is in a **hybrid** state:

- `GROUP_DATA` already exists and is used in `src/act_comm.c` (`group_create`, `group_add_member`, `group_remove_member`, `group_disband`, `group_sync_legacy_state`).
- `do_group` and `do_gtell` already prefer `group->members` iteration when available (better than full `loaded_chars` scans).
- Legacy fields (`leader`, `num_grouped`, `lgroup`) still exist and are synchronized for compatibility.
- `is_same_group` already checks `ach->group == bch->group` first, then falls back to leader logic.

Conclusion: we are **not** starting from implicit leader-only groups anymore; we are finishing a migration that already began.

## 2. Gap: Party UX and Consent Flow

Current group joining behavior is still immediate/legacy-oriented. We need explicit social flow for player parties:

- Leader invites outsider → outsider must accept/decline.
- Outsider requests join → leader must accept/decline.
- No forced modal prompts or interruptive spam.
- Prompt should show compact pending markers (similar to 2.0 ready-check indicator style).

## 3. Target UX

### 3.1 Commands

- `group` (existing): status list.
- `group invite <player>`: leader invites target player.
- `group request <leader>`: outsider requests to join leader's group.
- `group accept` / `group decline`: responder handles own pending invite.
- `group requests`: leader sees pending join requests.
- `group accept <player>` / `group decline <player>`: leader handles specific request.
- `group pending`: show personal pending invite/request metadata and remaining timeout.

### 3.2 Prompt Markers (non-interruptive)

Add concise markers to prompt (same spirit as old ready-check):

- `{Y[INV]{x` when player has an incoming invite.
- `{C[REQ]{x` when leader has pending join requests.

Markers should persist until resolved or expired, with normal command flow uninterrupted.

### 3.3 Notifications

- On create/expire/accept/decline, send one-line feedback.
- No repeated spam each pulse; rely on prompt marker for ambient awareness.

## 4. Data Model Additions

### 4.1 Per-player pending invite

Add to `PC_DATA`:

- inviter identity (`id[2]` and/or name snapshot)
- inviter group id snapshot
- expiry timestamp

Only one active incoming invite per player at a time (replace policy: newest wins, with notice).

### 4.2 Per-group join request queue

Add to `GROUP_DATA`:

- list of request entries `{requester_id, requester_name, created_at, expires_at}`

Cap requests per group to prevent abuse (e.g., 10 active).

### 4.3 Timeout policy

- Default TTL: 60s (configurable later).
- Expiry cleanup during periodic update pulse.

## 5. Behavior Rules

- Only players can invite/request (no NPC party invites).
- Leader-only invite authority.
- Cross-room allowed; cross-world restrictions follow existing grouping rules.
- Block invites/requests if either side is charmed, linkdead, or not in valid state.
- Respect existing group size limits.
- Invite acceptance re-validates all constraints at decision time (not only at send time).

## 6. Integration Points

- `src/act_comm.c`:
    - Extend `do_group` subcommand parser.
    - Add helper routines for enqueue/dequeue/resolve invite and request entries.
- Prompt rendering path (`src/comm.c`):
    - Append `[INV]` / `[REQ]` markers where other status tags are shown.
- Pulse/update path (`src/update.c` or existing periodic tick):
    - Expire stale invite/request entries and emit minimal notices.
- Optional script hooks (later):
    - `TRIG_GROUP_INVITE_SENT`, `TRIG_GROUP_INVITE_ACCEPTED`, etc.

## 7. Refactor Direction (after UX parity)

Once invite/request flow is stable:

- Continue reducing dependence on `leader` legacy semantics for party membership.
- Restrict `leader` to follower/charm compatibility or derive from `group->leader` consistently.
- Move grouping primitives into dedicated module (`groups.c/.h`) to reduce `act_comm.c` coupling.

## 8. Risks and Mitigations

- **Hybrid-state bugs** (group pointer vs leader mismatch):
    - Always call `group_sync_legacy_state()` after membership changes.
- **Request leak/stale markers**:
    - Centralize expiry cleanup and marker state calculation.
- **Command ambiguity**:
    - Keep subcommands explicit (`invite`, `request`, `accept`, `decline`, `requests`, `pending`).

## 9. Implementation Order

1. Add data structures for pending invite and group requests.
2. Add command handling for invite/request/accept/decline.
3. Add prompt markers `[INV]` / `[REQ]`.
4. Add expiry cleanup and notices.
5. Validate with focused tests/manual scenarios (simultaneous requests, leader swap, disband mid-pending).

## 10. Stretch Goals

- Ready-check command parity in current `src` (`readycheck start|yes|no|info`) integrated with same marker framework.
- Party role metadata (tank/healer/dps) and status indicators.
- LFG/group finder tooling.

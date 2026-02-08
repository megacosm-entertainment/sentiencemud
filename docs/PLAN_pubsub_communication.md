# PLAN: Communication System Refactor to Publish/Subscribe Model

This document outlines the feasibility and a high-level plan for refactoring the MUD's communication channels (gossip, ooc, yell, etc.) from the current iterative model to a more scalable and flexible Publish/Subscribe (Pub/Sub) model.

## 1. Current Communication Model

An analysis of the codebase, primarily in `act_comm.c`, reveals that the current communication system is implemented via direct, iterative delivery.

-   **Global/Area Channels (`gossip`, `yell`, `ooc`):** When a player sends a message on a channel, the system iterates through the entire global `descriptor_list` (all connected players). For each player, it performs a series of checks (e.g., `!IS_SET(victim->comm, COMM_NOGOSSIP)`, `!is_ignoring(...)`) to determine if the message should be delivered. This has a performance complexity of O(N), where N is the total number of players online.
-   **Room Channels (`say`):** These are more efficient, iterating only through the list of characters present in the sender's room. Complexity is O(M), where M is the number of occupants.
-   **Direct Channels (`tell`):** These involve a direct lookup of the recipient character, making them highly efficient after the initial O(1) lookup (assuming a hash map or similar structure for player lookup).

While functional, the current model's core limitation is the O(N) complexity for broadcast-style channels. As the player base grows, the CPU cycles spent iterating and checking conditions for every message sent on every channel will increase linearly, consuming valuable resources from the main game loop.

## 2. Proposed Publish/Subscribe Model

The proposed solution is to replace the iterative delivery mechanism with a unified, data-driven Publish/Subscribe pattern for *all* forms of communication, including global, room-local, and direct messages.

### High-Level Architecture

1.  **Topics & Streams:** Each channel will have a real-time topic (e.g., `rt:<topic>`) and a Redis Stream for history (e.g., `history:<topic>`). The `<topic>` will be dynamic based on the channel's `scope`.
    -   `GLOBAL` scope (`gossip`): `rt:gossip`
    -   `AREA` scope (`yell`): `rt:area_wv:<area_widevnum>` (where `<area_widevnum>` is the area's widevnum)
    -   `ROOM` scope (`say`): `rt:room_wv:<room_widevnum>` (where `<room_widevnum>` is the room's widevnum)
    -   `DIRECT_ENTITY` scope (`tell`): `rt:entity:<unique_id>` (where `<unique_id>` is the canonical identifier for the recipient entity)
    -   `GROUP_ENTITY` scope (`gtell`): `rt:group:<unique_id>` (where `<unique_id>` is the canonical identifier for the group entity)
    -   `CHURCH_ID` scope (`churchtalk`): `rt:church:<church_id>` (where `<church_id>` is the unique ID of the church)
2.  **Rich Message Payload:** When a message is sent, a rich payload is created. This payload is the single source of truth for the communication event.
    ```json
    {
      "timestamp": 1675881600,
      "sender_unique_id": "char_id_low:char_id_high",
      "sender_name": "Bob",
      "sender_long_desc": "A burly man with a long beard stands here.",
      "sender_room_widevnum": "area_uid:room_vnum",
      "sender_flag": "[My Flag]",
      "message_text": "Hello there!"
    }
    ```
3.  **Publishing:** The generic channel handler will:
    a.  **Persist:** `XADD` the rich payload to the appropriate history stream. This operation returns a unique Stream ID for the message, which can be used for features like reporting.
    b.  **Publish:** `PUBLISH` a reference to the message to the appropriate real-time topic.
4.  **Subscribing & Receiving:**
    -   Players subscribe to topics based on their context. A player in room 456 subscribes to `rt:room_wv:<room_widevnum>`. When they move rooms, they unsubscribe and subscribe to the new room's topic. They are always subscribed to their own direct entity topic (`rt:entity:<player_unique_id>`).
    -   A central listener receives the real-time notification. Before displaying the message, it performs final checks like `is_ignoring()`. It then uses the data in the payload and the channel's formatting rules to construct and display the final message to the recipient.

This architecture provides a consistent mechanism for all communication, enabling universal features like history and timestamping while handling contextual complexity.

## 3. Feasibility Analysis

### Advantages

-   **Unified System:** All communication, from `gossip` to `say` to `tell`, uses the same underlying pipeline. This reduces code duplication and makes the system easier to reason about and maintain.
-   **Universal Features:** Channel history and accurate timestamping can be implemented for *everything*, including `say` and `tell`, which is a significant feature enhancement.
-   **Scalability:** The performance benefits still apply to broadcast channels.
-   **Flexibility & Decoupling:** The advantages of decoupling are now even greater, as more of the system leverages this pattern.

### New Features Enabled by this Refactor

-   **Universal Channel History:** A player can see the history not just of `gossip`, but also what was said in a room before they entered, or their last few `tell`s.
-   **Universal Timestamping:** Every message, including `say`, is accurately timestamped.
-   **Message Reporting:** The persistent nature of Redis Streams, coupled with unique message IDs generated by `XADD`, provides a robust foundation for implementing an in-game message reporting system. This allows players to report inappropriate messages, with staff able to easily retrieve the exact message content and context.
-   **Content Filtering and Automated Reporting:** The centralized nature of message processing, coupled with persistent storage, allows for the integration of a content filtering service. This service can analyze message payloads for problematic content (e.g., keywords, patterns). Depending on the severity, it can:
    *   Redact or block messages in real-time and place the original message, with full context, into a dedicated **Staff Review Queue** for human oversight.
    *   Automatically generate and submit reports to staff (similar to manual player reports), also often routing them to the Staff Review Queue.
    *   Trigger automated warnings or temporary mutes for repeat offenders.

### Challenges

-   **Subscription Management:** This is now more complex, as players must constantly update their `ROOM` and `AREA` subscriptions as they move. This must be a highly efficient part of the movement code.
-   **Payload Size & Serialization:** Sending a rich JSON payload for every single message, including every line of `say`, increases overhead compared to the current simple string passing. The performance impact of Jansson serialization/deserialization at this scale must be tested.
-   **Initial Implementation Effort:** This is a more significant refactor than originally scoped, touching almost all communication code.

### Technical Considerations for Unique Identifiers

-   **The `id`/`id2` Challenge:** The current system uses two integer fields (`id` and `id2`) for unique identification of entities (characters, objects, rooms, etc.). For a consistent Pub/Sub model, a single, canonical `unique_id` is highly desirable for generating Redis topics (e.g., `rt:entity:<unique_id>`) and for payload fields (`sender_unique_id`).
-   **Proposed Solution:** A consistent strategy must be adopted to derive a single `unique_id` from `id` and `id2`. This could involve:
    1.  **String Concatenation:** Combining `sprintf(buf, "%ld:%ld", id, id2)` to create a string identifier. This is human-readable and provides a direct mapping.
    2.  **64-bit Integer Mapping:** If `id` and `id2` are 32-bit integers, they could potentially be combined into a single 64-bit integer (`(id << 32) | id2`). This would be highly efficient for Redis keys.
    3.  **Redis-Managed UIDs:** Assigning a unique string or integer ID directly managed by Redis for each entity, and mapping the MUD's `id`/`id2` to this Redis UID internally. This adds complexity but offers maximum flexibility.
-   **Consistency is Key:** Whichever method is chosen, it must be applied consistently across the MUD code when interacting with the Pub/Sub system for entity identification. This `unique_id` will be used for things like `is_ignoring()` checks and `DIRECT_ENTITY` topics.


-   **Group Identification:** Since groups are not currently a distinct entity type with their own unique IDs, a full refactor of the group system is a larger, separate project. For this pub/sub rework, an interim strategy must be used. The recommended approach is to use the `unique_id` of the group's **leader** as the identifier for the group's channel topic (e.g., `rt:group:<leader's_unique_id>`). This provides a stable identifier for the group's lifetime, with the understanding that a change in leadership will change the group's channel topic.

## 4. Advanced Channel Configuration & Editor

To manage this complexity, a powerful, data-driven system is essential.

### Channel Definition Structure

The "Channel Definition" becomes the brain of the system.

```json
{
  "id": "say",
  "name": "Say",
  "command": "say",
  "scope": "ROOM_WV", // Indicating widevnum usage
  "allow_player_flags": false,
  "permissions": { "speak": "ALL" },
  "formatting": {
    "self": "{CYou say, '$t'{x",
    "receiver": "{C$n says, '$t'{x"
  },
  "modifiers": ["PUNCTUATION_PARSE"]
}
```
-   **`scope`**: The most important new field. `GLOBAL`, `AREA_WV` (for widevnum areas), `ROOM_WV` (for widevnum rooms), `DIRECT_ENTITY` (for entities identified by their `unique_id`), `GROUP_LEADER` (for group-specific channels, identified by the group leader's `unique_id`), `CHURCH_ID` (for church-specific channels identified by a church ID). This dictates how topics are generated and who receives messages.
-   **`allow_player_flags`**: A boolean to control whether player-set channel flags can be used. This would be `true` for `gossip` but `false` for `say`.
-   **`permissions`**: For `GROUP_ENTITY` and `CHURCH_ID` scoped channels, the `permissions` field is crucial. It would specify that only `GROUP_MEMBER` or `CHURCH_MEMBER` (or higher ranks within them) can speak and/or listen, ensuring private communication within these entities.
-   **`modifiers`**: Can now include `PUNCTUATION_PARSE`, which would analyze the message text and dynamically swap the `formatting` string (e.g., from "says" to "asks").

### Handling `ignore`, `qlist`, and `shape`

-   **`ignore` & `qlist`:** These checks are performed by the central pub/sub listener on the game server *before* delivering a message to a recipient. The listener gets the `sender_id` from the message payload and checks it against the recipient's ignore list. This keeps the logic within the MUD's C code, where it belongs.
-   **`shape` (Identity):** The receiver always uses the `sender_name` and `sender_long_desc` from the payload. This ensures that if a shaped-character "Mob A" says something, the message is correctly attributed to "Mob A" for all recipients, even if the character has since changed shape.

### Channel Editor (`cedit`)

The `cedit` OLC editor becomes even more critical in this model for managing the more complex channel definitions.

## 5. High-Level Implementation Plan

The plan is adjusted to reflect the unified approach.

1.  **Phase 1: Core Channel Service & Editor**
    a.  **Design Data Structures:** Implement C structs for the enriched `ChannelDefinition` and the rich message payload.
    b.  **Implement JSON Serialization:** For both channel definitions and message payloads.
    c.  **Build `cedit` OLC.**
    d.  **Implement Content Filtering Service:** Develop modules for analyzing message text for problematic content. This service will intercept messages within the generic handler, apply filtering rules, and determine actions (redaction, blocking, automated reporting). Critically, messages flagged for review or automatic reporting will be `XADD`ed to a dedicated **Staff Review Queue Redis Stream** (e.g., `audit:filtered_messages`).
    e.  **Create Generic Channel Handler:** This is the core function. It takes a sender, a channel `id`, and a message. It looks up the definition, applies content filtering, builds the payload, and publishes it.
    f.  **Develop Staff Review Tool (`rview`):** Create a new immortal command or OLC editor for staff to easily access, review, and act upon messages in the Staff Review Queue.

2.  **Phase 2: Pub/Sub Infrastructure**
    a.  **Implement Pub/Sub Listener:** Create the Redis client that subscribes to topics (using wildcards like `rt:*`) and receives notifications.
    b.  **Implement Receiver Logic:** Code the logic that, upon receiving a message, fetches the full payload from the Stream, checks `ignore`, and formats/sends the message to the player.
    c.  **Implement Message Reporting System:** Develop the `report` command (e.g., `report <channel> <message_id> <reason>`) that leverages the unique Stream IDs to retrieve reported messages. Create a system to log these reports and notify staff.

3.  **Phase 3: Migration of Global Channels**
    -   Use `cedit` to define `gossip`, `ooc`, etc.
    -   Refactor their `do_*` functions to call the generic channel handler. This serves as the first real-world test of the system.

4.  **Phase 4: Migration of Contextual & Direct Channels**
    -   Implement subscription management for movement, using `AREA_WV` (widevnum areas) and `ROOM_WV` (widevnum rooms) for topics.
    -   Implement subscription management for `GROUP_LEADER` channels. This involves:
        *   Subscribing/unsubscribing members when they join/leave a group, using the leader's `unique_id` to identify the topic.
        *   Handling leadership changes by having all members unsubscribe from the old leader's topic and re-subscribe to the new leader's topic.
    -   Implement subscription management for `CHURCH_ID` channels (joining/leaving churches).
    -   Refactor `do_say`, `do_yell`, `do_tell` (using `DIRECT_ENTITY` with `unique_id`), `do_gtell` (using `GROUP_LEADER` with leader's `unique_id`), `do_churchtalk` (using `CHURCH_ID`), etc., to use the generic handler.

## 6. Channel Moderation System

The current moderation model is blunt: `COMM_NOCHANNELS` silences a character on *all* channels, and `COMM_NOTELL` blocks all tells. There are no per-channel mutes, no moderator roles below immortal, and no audit trail linking penalties to specific incidents. The chat room system has ops and a `CHAT_BAN_DATA` structure, but `do_chat_ban` is unimplemented.

The pubsub refactor creates an opportunity to build a proper moderation system that supports per-channel penalties, moderator roles, and account-level enforcement with an audit trail.

### 6.1 Penalty Scope: Character vs. Account

Penalties can target either a character or an entire account. This distinction matters because a character-level mute on `gossip` doesn't stop the player from logging an alt and continuing the behavior.

| Scope | Applies To | Use Case |
|-------|-----------|----------|
| Character | Single `CHAR_DATA` | Minor infractions, roleplaying-context penalties, first offenses |
| Account | All characters on the `ACCOUNT_DATA` | Repeat offenders, serious violations, evasion of character-level penalties |

The account system already has the infrastructure for this:
- `ACCOUNT_DATA` has a `bans` field (`BAN_DATA *`) and a commented-out `PENALTY_DATA *penalties`
- `ACCOUNT_DATA` has `staff_notes` for audit trail documentation
- Characters are linked to accounts via the `characters` list

The commented-out `PENALTY_DATA` in `ACCOUNT_DATA` is the natural home for this. Uncomment and expand it.

### 6.2 Penalty Data Structure

```c
typedef struct channel_penalty_data CHANNEL_PENALTY_DATA;

struct channel_penalty_data
{
    CHANNEL_PENALTY_DATA *next;
    char      *channel_id;      /* channel name, or "*" for all channels */
    char      *target_name;     /* character name, or NULL for account-wide */
    int        penalty_type;    /* CHANPEN_MUTE, CHANPEN_BAN, CHANPEN_WARN */
    int        scope;           /* PENSCOPE_CHARACTER or PENSCOPE_ACCOUNT */
    time_t     issued_at;       /* when the penalty was applied */
    time_t     expires_at;      /* 0 = permanent, otherwise unix timestamp */
    char      *issued_by;       /* staff member or "SYSTEM" for automated */
    char      *reason;          /* human-readable reason */
    char      *incident_ref;    /* Redis Stream message ID that triggered this, if any */
};

#define CHANPEN_WARN    0   /* warning — logged but not enforced */
#define CHANPEN_MUTE    1   /* cannot speak on the channel */
#define CHANPEN_BAN     2   /* cannot speak or listen on the channel */

#define PENSCOPE_CHARACTER  0
#define PENSCOPE_ACCOUNT    1
```

### 6.3 Where Penalties Live

```
ACCOUNT_DATA
  ├── staff_notes          (existing — audit trail for staff commentary)
  ├── penalties            (new — CHANNEL_PENALTY_DATA linked list)
  │     ├── { channel: "gossip", target: "Bob", type: MUTE, scope: CHARACTER, expires: 1707500000 }
  │     ├── { channel: "*", target: NULL, type: MUTE, scope: ACCOUNT, expires: 0 }
  │     └── { channel: "ooc", target: "Bob", type: WARN, scope: CHARACTER, expires: 0 }
  └── bans                 (existing — site/IP bans, separate concern)
```

Account notes (`staff_notes` / `accnote` command) provide the narrative audit trail: "Muted Bob on gossip for repeated harassment, see incident report #12345." Penalty records provide the enforceable data.

### 6.4 Enforcement in the Pub/Sub Pipeline

Penalties are checked at **publish time** (before the message enters the pipeline) and optionally at **subscribe time** (for BAN-level penalties that also block listening).

```
Player types "gossip hello"
        │
        ▼
  Generic Channel Handler
        │
        ├── Check channel_penalty for this character
        │     └── Found CHANPEN_MUTE on "gossip" for "Bob", not expired
        │           └── "You are muted on gossip until <date>. Reason: <reason>"
        │           └── STOP — message never enters pipeline
        │
        ├── (if no character penalty) Check account-level penalty
        │     └── Found CHANPEN_MUTE on "*" for account, scope ACCOUNT
        │           └── "Your account is muted on all channels. Reason: <reason>"
        │           └── STOP
        │
        ├── Content filter (existing plan)
        │     └── May auto-generate a CHANPEN_WARN or CHANPEN_MUTE
        │
        ▼
  PUBLISH to topic
```

For BAN-level penalties, the subscription manager also checks: when a player connects or a penalty is applied, `rt:gossip` is unsubscribed so they don't even see messages on that channel.

### 6.5 Moderator Roles

Currently, only immortals can apply penalties (`COMM_NOCHANNELS` via `do_nochannels`). The pubsub system should support delegated moderation:

#### Role Definitions

| Role | Can Do | Scope |
|------|--------|-------|
| **Player** | `ignore` (self-moderation), `report` a message | Own experience only |
| **Channel Moderator** | `chanmute`, `chanwarn`, view recent reports for their channel | Specific channels they moderate |
| **Staff (Immortal)** | All of the above, plus `chankick`, `chanban`, account-wide penalties, penalty removal, review queue access | All channels |
| **Senior Staff** | All of the above, plus permanent bans, penalty policy changes | All channels + policy |

#### Channel Moderator Assignment

The channel definition (managed via `cedit`) gains a moderators field:

```json
{
  "id": "gossip",
  "name": "Gossip",
  "scope": "GLOBAL",
  "moderators": {
    "role": "CHANNEL_MOD",
    "members": ["Alice", "Charlie"],
    "max_penalty_duration": 86400
  }
}
```

Channel moderators are trusted players who can issue short-duration mutes (up to `max_penalty_duration`) on their assigned channels. They cannot issue account-wide penalties or permanent bans. This mirrors the chat room ops (`CHAT_OP_DATA`) pattern but extends it to global channels.

#### Moderator Commands

```
chanwarn <channel> <player> <reason>    — Log a warning (no enforcement, creates audit record)
chanmute <channel> <player> <duration> <reason> — Mute a player on a specific channel
chanban <channel> <player> <duration> <reason>  — Ban a player from a channel (staff only)
chanpenalty list <player>               — View active penalties for a player
chanpenalty remove <player> <#>         — Remove a penalty (staff only)
chanpenalty history <player>            — View all penalties including expired (staff only)
```

### 6.6 Graduated Enforcement

Penalties escalate automatically based on history:

```
1st offense  →  Warning (CHANPEN_WARN) — logged, no enforcement
2nd offense  →  Short mute (CHANPEN_MUTE, 15 min) — character-scoped
3rd offense  →  Long mute (CHANPEN_MUTE, 24 hr) — character-scoped
4th offense  →  Account mute (CHANPEN_MUTE, 48 hr) — account-scoped
5th+ offense →  Staff review — flagged for manual action, possible permanent ban
```

The content filter (already in the plan) can trigger the first steps automatically. The escalation schedule is configurable per channel — `gossip` might have stricter enforcement than an `ooc` channel.

Escalation checks the penalty history on the account:
```c
int count_recent_penalties(ACCOUNT_DATA *acct, const char *channel_id, time_t window) {
    int count = 0;
    for (CHANNEL_PENALTY_DATA *pen = acct->penalties; pen; pen = pen->next) {
        if ((!channel_id || !str_cmp(pen->channel_id, channel_id) || !str_cmp(pen->channel_id, "*"))
            && pen->issued_at > (current_time - window))
            count++;
    }
    return count;
}
```

### 6.7 Integration with Account Notes

When a penalty is issued, an account note is automatically created via the existing `accnote` system:

```c
void penalty_create_audit_note(ACCOUNT_DATA *acct, CHANNEL_PENALTY_DATA *pen) {
    ACCOUNT_NOTE_DATA *note = alloc_mem(sizeof(ACCOUNT_NOTE_DATA));
    note->author = str_dup(pen->issued_by);
    note->subject = str_dup(formatf("PENALTY: %s %s on [%s]",
        pen->scope == PENSCOPE_ACCOUNT ? "ACCOUNT" : "CHAR",
        pen->penalty_type == CHANPEN_MUTE ? "MUTE" :
        pen->penalty_type == CHANPEN_BAN  ? "BAN"  : "WARN",
        pen->channel_id));
    note->text = str_dup(formatf("Target: %s\nReason: %s\nDuration: %s\nIncident: %s\n",
        pen->target_name ? pen->target_name : "(account-wide)",
        pen->reason,
        pen->expires_at ? formatf("%ld seconds", pen->expires_at - pen->issued_at) : "permanent",
        pen->incident_ref ? pen->incident_ref : "manual"));
    note->timestamp = current_time;
    note->next = acct->staff_notes;
    acct->staff_notes = note;
}
```

This gives staff a unified view: `accnote list <account>` shows both free-form staff commentary and structured penalty records in chronological order. The `incident_ref` field links back to the Redis Stream message ID from the content filter or player report, so staff can pull up the exact message that triggered the penalty.

### 6.8 Persistence

Penalties are saved as part of the account JSON file:

```json
{
  "username": "bobplayer",
  "penalties": [
    {
      "channel": "gossip",
      "target": "Bob",
      "type": "mute",
      "scope": "character",
      "issued_at": 1707400000,
      "expires_at": 1707486400,
      "issued_by": "Alice",
      "reason": "Repeated spam",
      "incident_ref": "1707399950000-0"
    }
  ]
}
```

Expired penalties are not automatically deleted — they remain in the history for escalation checks and audit purposes. A periodic cleanup can archive penalties older than a configurable retention window (e.g., 90 days).

### 6.9 Relationship to Existing Systems

| Existing | Replaced By | Notes |
|----------|------------|-------|
| `COMM_NOCHANNELS` | Account-wide `CHANPEN_MUTE` on `"*"` | Equivalent effect, but with reason, duration, and audit trail |
| `COMM_NOTELL` | `CHANPEN_BAN` on `"tell"` channel | Tells become a pubsub channel too |
| `do_nochannels` (staff cmd) | `chanmute * <player> <duration> <reason>` | Same effect, more granular |
| Chat room ops (`CHAT_OP_DATA`) | Channel moderator role | Generalized from chat rooms to all channels |
| Chat room bans (`CHAT_BAN_DATA`) | `CHANNEL_PENALTY_DATA` with `CHANPEN_BAN` | Unified penalty model |
| Player `ignore` list | Unchanged — self-moderation remains separate | `IGNORE_DATA` is a personal preference, not a penalty |

---

## 7. System Integration & Scope

-   **Goal:** The ultimate goal is to migrate all player-to-player communication channels (`gossip`, `ooc`, `say`, `tell`, `gtell`, `yell`, `churchtalk`, etc.) to this unified system.
-   **Out of Scope:** Purely client-side commands (`clear`) or visual emotes (`do_emote`) that do not have a concept of a "channel" or targeted recipients would not be part of this system and would remain as-is.

By adopting this unified, data-driven model, we not only gain scalability but also create a far more consistent, maintainable, and feature-rich communication system for the future.

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

### Runtime Constraint (Hard Requirement)

-   The main game loop remains single-threaded and must never block on channel I/O.
-   Any blocking transport operations (e.g., Redis `SUBSCRIBE`/network reads) run in dedicated worker thread(s), similar to existing cache/email background patterns.
-   Cross-thread handoff uses bounded queues:
  -   outbound queue: game loop -> transport worker
  -   inbound queue: transport worker -> game loop
-   The game loop only does non-blocking queue push/pop and local message processing.
-   On queue pressure or backend failure, prefer dropping/degrading transport-side features over stalling gameplay.

### High-Level Architecture

1.  **Topics & Streams:** Each channel will have a real-time topic (e.g., `rt:<topic>`) and a Redis Stream for history (e.g., `history:<topic>`). The `<topic>` will be dynamic based on the channel's `scope`.
    -   `GLOBAL` scope (`gossip`): `rt:gossip`
    -   `AREA` scope (`yell`): `rt:area:<area_uid>` (where `<area_uid>` is the area's UID, or a custom topic override — see below)
  -   `REGION` scope (`regionchat`/continent channels): `rt:region:<region_uid>`
    -   `ROOM` scope (`say`): `rt:room_wv:<room_widevnum>` (where `<room_widevnum>` is the room's widevnum)
    -   `DIRECT_ENTITY` scope (`tell`): `rt:entity:<unique_id>` (where `<unique_id>` is the canonical identifier for the recipient entity)
    -   `GROUP_ID` scope (`gtell`): `rt:group:<group_uid>` (where `<group_uid>` is derived from `GROUP_DATA->id[0]:id[1]`)
    -   `CHURCH_ID` scope (`churchtalk`): `rt:church:<church_id>` (where `<church_id>` is the unique ID of the church)
  -   **Multi-target coverage:** Channel definitions may optionally route to multiple area/region targets (or topic groups) for continent-wide and federation channels.
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
  -   A transport listener worker receives real-time notifications and enqueues compact events for the game loop. The game loop drains inbound events each pulse, performs final checks like `is_ignoring()`, then formats/displays the message.

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
-   **Legacy Split Zones:** Some legacy areas are split across two or more zone files due to historical vnum overlap issues (which the widevnum system resolves going forward). These logically represent a single area — a `yell` in one half should be heard in the other. Areas must be able to specify a custom `area_topic` override so that multiple zone files can share the same `AREA` pub/sub topic. When set, this overrides the default `rt:area:<area_uid>` topic with `rt:area:<area_topic>`. When unset, the area's own UID is used as the topic key. This is an `aedit` field on the area definition.

### Technical Considerations for Unique Identifiers

-   **The `id`/`id2` Challenge:** The current system uses two integer fields (`id` and `id2`) for unique identification of entities (characters, objects, rooms, etc.). For a consistent Pub/Sub model, a single, canonical `unique_id` is highly desirable for generating Redis topics (e.g., `rt:entity:<unique_id>`) and for payload fields (`sender_unique_id`).
-   **Proposed Solution:** A consistent strategy must be adopted to derive a single `unique_id` from `id` and `id2`. This could involve:
    1.  **String Concatenation:** Combining `sprintf(buf, "%ld:%ld", id, id2)` to create a string identifier. This is human-readable and provides a direct mapping.
    2.  **64-bit Integer Mapping:** If `id` and `id2` are 32-bit integers, they could potentially be combined into a single 64-bit integer (`(id << 32) | id2`). This would be highly efficient for Redis keys.
    3.  **Redis-Managed UIDs:** Assigning a unique string or integer ID directly managed by Redis for each entity, and mapping the MUD's `id`/`id2` to this Redis UID internally. This adds complexity but offers maximum flexibility.
-   **Consistency is Key:** Whichever method is chosen, it must be applied consistently across the MUD code when interacting with the Pub/Sub system for entity identification. This `unique_id` will be used for things like `is_ignoring()` checks and `DIRECT_ENTITY` topics.


-   **Group Identification:** Groups now have their own identity via `GROUP_DATA->id[0]` and `GROUP_DATA->id[1]`. For pub/sub routing, the canonical group topic key should be derived from that pair (e.g., `rt:group:<id0>:<id1>`). This avoids topic churn on leadership changes and provides a true group-scoped identity.

## 4. Advanced Channel Configuration & Editor

To manage this complexity, a powerful, data-driven system is essential.

### Channel Definition Structure

The "Channel Definition" becomes the brain of the system.

```json
{
  "id": "say",
  "name": "Say",
  "command": "say",
  "scope": "ROOM_WV",
  "allow_player_flags": false,
  "permissions": { "speak": "ALL" },
  "formatting": {
    "self": "{CYou say, '$t'{x",
    "receiver": "{C$n says, '$t'{x"
  },
  "modifiers": ["PUNCTUATION_PARSE"]
}
```
-   **`scope`**: The most important new field. `GLOBAL`, `AREA` (using area UIDs), `REGION` (using region UIDs), `ROOM_WV` (using room widevnums), `DIRECT_ENTITY` (for entities identified by their `unique_id`), `GROUP_ID` (for group-specific channels, identified by `GROUP_DATA->id`), `CHURCH_ID` (for church-specific channels identified by a church ID). This dictates how topics are generated and who receives messages.
-   **`route_targets`**: Optional routing set for channels that should span multiple regions/areas/topic-groups.

```json
{
  "id": "continent",
  "scope": "REGION",
  "route_targets": {
    "regions": [1001, 1002, 1003],
    "areas": [],
    "topic_groups": ["continent:eldara"]
  }
}
```
-   **`allow_player_flags`**: A boolean to control whether player-set channel flags can be used. This would be `true` for `gossip` but `false` for `say`.
-   **`permissions`**: For `GROUP_ID` and `CHURCH_ID` scoped channels, the `permissions` field is crucial. It would specify that only `GROUP_MEMBER` or `CHURCH_MEMBER` (or higher ranks within them) can speak and/or listen, ensuring private communication within these entities.
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
  a.  **Implement Pub/Sub Listener Worker:** Create a dedicated Redis listener worker that subscribes to topics (using wildcards like `rt:*`) and pushes events to a thread-safe inbound queue.
    b.  **Implement Receiver Logic:** Code the logic that, upon receiving a message, fetches the full payload from the Stream, checks `ignore`, and formats/sends the message to the player.
    c.  **Implement Message Reporting System:** Develop the `report` command (e.g., `report <channel> <message_id> <reason>`) that leverages the unique Stream IDs to retrieve reported messages. Create a system to log these reports and notify staff.

3.  **Phase 3: Migration of Global Channels**
    -   Use `cedit` to define `gossip`, `ooc`, etc.
    -   Refactor their `do_*` functions to call the generic channel handler. This serves as the first real-world test of the system.

4.  **Phase 4: Migration of Contextual & Direct Channels**
    -   Implement subscription management for movement, using `AREA` (area UIDs), `REGION` (region UIDs), and `ROOM_WV` (room widevnums) for topics.
    -   Implement subscription management for `GROUP_ID` channels. This involves:
      *   Subscribing/unsubscribing members when they join/leave a group, using `GROUP_DATA->id` as the topic identity.
      *   Handling group create/disband and membership transitions so topic membership follows current group composition.
    -   Implement subscription management for `CHURCH_ID` channels (joining/leaving churches).
    -   Refactor `do_say`, `do_yell`, `do_tell` (using `DIRECT_ENTITY` with `unique_id`), `do_gtell` (using `GROUP_ID` with `GROUP_DATA->id`), `do_chtalk` (using `CHURCH_ID`), and region channels (`REGION`) to use the generic handler.

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

### 6.9 Player Word Filters (Personal Content Filtering)

Separate from staff-enforced moderation, players can define their own word/phrase filters for content that isn't against the rules but that they personally don't want to see. Filtered words are replaced inline rather than suppressing the entire message.

This sits alongside the existing `IGNORE_DATA` system on `pc_data`:
- **Ignore** = "I don't want messages from this *person*"
- **Word filter** = "I don't want to see this *content* from anyone"

#### Data Structure

```c
typedef struct word_filter_data WORD_FILTER_DATA;

struct word_filter_data
{
    WORD_FILTER_DATA *next;
    char *pattern;          /* word or phrase to match (case-insensitive) */
    char *replacement;      /* what to display instead, or NULL for default mask */
};
```

Stored on `pc_data` alongside `ignoring`:

```c
struct pc_data
{
    ...
    IGNORE_DATA     *ignoring;
    WORD_FILTER_DATA *word_filters;   /* player-defined content filters */
    char            *filter_mask;     /* default replacement string, e.g. "****" */
    ...
};
```

#### How It Works

Filtering happens at **display time** on the receiver's side — after the message exits the pub/sub pipeline but before it's sent to the player's descriptor. The original message in the Redis Stream is never modified.

```c
/* Applied in the receiver's message formatting, after ignore checks */
char *apply_word_filters(PC_DATA *pcdata, const char *message) {
    char *result = str_dup(message);

    for (WORD_FILTER_DATA *wf = pcdata->word_filters; wf; wf = wf->next) {
        /* Case-insensitive search and replace */
        result = str_replace_ci(result, wf->pattern,
            wf->replacement ? wf->replacement : pcdata->filter_mask);
    }

    return result;
}
```

#### Player Commands

```
filter add <word/phrase> [replacement]  — Add a filter. If no replacement, uses default mask
filter remove <word/phrase>             — Remove a filter
filter list                             — Show current filters
filter mask <string>                    — Set the default replacement (default: "***")
```

Examples:
```
> filter add "pineapple pizza" [REDACTED FOR TASTE]
Filter added: "pineapple pizza" → "[REDACTED FOR TASTE]"

> filter add poop
Filter added: "poop" → "***"

> filter mask ####
Default filter mask set to: "####"
```

#### Design Considerations

**1. Replacement is per-filter, with a fallback default.**
A player might want profanity replaced with `"***"` but a specific topic replaced with something humorous. Per-filter replacements allow this while the `filter_mask` default keeps simple cases simple.

**2. Filters apply to all channels uniformly.**
There's no per-channel filter — if a player filters a word, it's filtered everywhere. This keeps the system simple. If per-channel filtering is needed later, the `WORD_FILTER_DATA` struct can gain a `channel_id` field.

**3. Matching is case-insensitive and substring-based.**
Filtering "poop" also catches "POOP" and "poopy." This prevents trivial circumvention via capitalization. More sophisticated pattern matching (regex, l33tspeak normalization) could be added later but isn't needed initially.

**4. Original messages are never altered.**
The filter runs on the receiver's copy of the message text. Other players see the original. The Redis Stream history retains the unmodified message — staff reviewing reports see what was actually said.

**5. Persistence.**
Word filters are saved in the player's character file as part of `pc_data`, similar to how `IGNORE_DATA` is persisted today.

### 6.10 Relationship to Existing Systems

| Existing | Replaced By | Notes |
|----------|------------|-------|
| `COMM_NOCHANNELS` | Account-wide `CHANPEN_MUTE` on `"*"` | Equivalent effect, but with reason, duration, and audit trail |
| `COMM_NOTELL` | `CHANPEN_BAN` on `"tell"` channel | Tells become a pubsub channel too |
| `do_nochannels` (staff cmd) | `chanmute * <player> <duration> <reason>` | Same effect, more granular |
| Chat room ops (`CHAT_OP_DATA`) | Channel moderator role | Generalized from chat rooms to all channels |
| Chat room bans (`CHAT_BAN_DATA`) | `CHANNEL_PENALTY_DATA` with `CHANPEN_BAN` | Unified penalty model |
| Player `ignore` list | Unchanged — self-moderation remains separate | `IGNORE_DATA` is a personal preference, not a penalty |

### 6.11 Scripting Integration & Room-Scoped TTL

The scripting engine fires `TRIG_SPEECH`, `TRIG_SAYTO`, and `TRIG_WHISPER` triggers on entities present in the room — mobs, objects (in inventory, worn, and on the ground), and the room itself. These triggers are the backbone of NPC interaction (e.g., quest NPCs responding to keywords).

In the current model, `do_say` iterates the room's people and contents lists and fires `p_act_trigger()` synchronously. In the pub/sub model, room-scoped messages are published to `rt:room_wv:<room_widevnum>`. For script triggers to continue working, all scriptable entities in a room must effectively be subscribers to that room's topic.

#### Approach

-   **Room entities as implicit subscribers.** When processing an incoming message on a `ROOM_WV` topic, the receiver logic must iterate the room's entity lists and fire `p_act_trigger()` for each — exactly as `do_say` does today. The pub/sub layer handles delivery; the trigger-firing remains local to the game loop.
-   **This is not a Redis subscription per mob.** NPCs and objects don't get their own Redis subscriptions. The game server's single subscriber for `rt:room_wv:*` handles all rooms. On receipt, it looks up the room and fires triggers on its occupants. This keeps the Redis subscription count bounded.
-   **Trigger ordering.** Script triggers must fire *after* the message is displayed to players (preserving current behavior where NPC responses appear after the player's speech). The receiver logic should: (1) deliver to player descriptors, (2) fire `p_act_trigger()` on room entities.

#### Room-Scoped TTL

Room-scoped chat (`say`, `whisper`, `sayto`, `intone`) generates far more volume than global channels and has limited historical value. The Redis Streams for `ROOM_WV` topics should use a low TTL:

-   **`MAXLEN` or `MINID`:** Use `XADD ... MAXLEN ~ 50` or time-based trimming to keep only the last ~50 messages or ~15 minutes of history per room. This provides enough for "what was just said?" while preventing unbounded growth.
-   **Configurable per channel definition:** The channel definition gains a `history` field:
    ```json
    {
      "id": "say",
      "scope": "ROOM_WV",
      "history": {
        "max_len": 50,
        "max_age_seconds": 900
      }
    }
    ```
-   **Global channels** (`gossip`, `ooc`) can retain longer history (e.g., 24 hours or 1000 messages). Direct channels (`tell`) might retain a moderate window. This is all configurable via `cedit`.
-   **Trimming strategy:** `XTRIM` can be called periodically (e.g., on a pulse timer) or piggy-backed on `XADD` with approximate `MAXLEN ~`. The approximate form is cheap and prevents streams from growing unbounded between cleanup cycles.

---

## 7. System Integration & Scope

-   **Goal:** The ultimate goal is to migrate all player-to-player communication channels (`gossip`, `ooc`, `say`, `tell`, `gtell`, `yell`, `churchtalk`, etc.) to this unified system.
-   **Out of Scope:** Purely client-side commands (`clear`) or visual emotes (`do_emote`) that do not have a concept of a "channel" or targeted recipients would not be part of this system and would remain as-is.

By adopting this unified, data-driven model, we not only gain scalability but also create a far more consistent, maintainable, and feature-rich communication system for the future.

## 8. Operation Without Redis (Fallback / Degraded Mode)

Redis should be treated as a **transport backend**, not the channel system itself. The channel core (definitions, permissions, moderation, filtering, formatting, script trigger sequencing) must continue to work if Redis is unavailable.

### 8.1 Transport Abstraction

Implement a narrow transport interface used by the generic channel handler and receiver logic:

```c
typedef struct channel_transport CHANNEL_TRANSPORT;

struct channel_transport {
  bool (*publish)(const char *topic, const CHANNEL_MESSAGE *msg);
  bool (*subscribe)(const char *topic);
  bool (*unsubscribe)(const char *topic);
  bool (*append_history)(const char *stream, const CHANNEL_MESSAGE *msg, char *out_id, size_t out_id_sz);
  int  (*fetch_history)(const char *stream, const char *cursor, int limit, CHANNEL_MESSAGE_LIST *out);
  bool (*health_check)(void);
  const char *name;
};
```

Backends:
- **RedisTransport**: Uses `XADD`, `PUBLISH`, `SUBSCRIBE`, `XREAD`/`XRANGE`.
- **LocalTransport**: In-process topic registry + per-topic ring buffer history.
- **LegacyIterativeTransport** (optional safety net): Uses existing direct iteration path for emergency fallback.

### 8.2 LocalTransport Behavior

When Redis is down or disabled, LocalTransport provides single-process pub/sub semantics:

- **Topics:** In-memory subscriber sets keyed by generated topic name (same naming rules as Redis mode).
- **History:** Per-topic ring buffers with channel-configured retention (`max_len`, `max_age_seconds`).
- **Message IDs:** Locally generated IDs (e.g., `<epoch_ms>-<sequence>`) compatible with report/audit references.
- **Delivery Path:** Queue messages into the main game loop and run normal receiver checks (`ignore`, penalties, word filters, formatting) without blocking.
- **Room Script Triggers:** Preserve ordering: player delivery first, then trigger firing (`p_act_trigger()`).

This keeps behavior consistent with the planned architecture while removing external dependency at runtime.

### 8.3 Failover and Recovery Policy

Add backend mode setting:

```text
channel_backend = auto | redis | local | legacy_iterative
```

- **auto (recommended):** Start on Redis; if health checks or publish/history operations fail repeatedly, trip a circuit breaker and switch to LocalTransport.
- **redis:** Require Redis; if unavailable, channels report degraded errors (useful for strict staging checks).
- **local:** Never attempt Redis (single-node offline operation).
- **legacy_iterative:** Explicit fallback to pre-pubsub delivery when needed.

Recovery behavior for `auto`:
- Probe Redis periodically with cooldown/hysteresis to avoid rapid backend flapping.
- On stable recovery, switch back to RedisTransport.
- Log backend transitions (`LOG_WARN`/`LOG_INIT`) for staff visibility.

### 8.4 Feature Impact Matrix When Redis Is Unavailable

| Capability | RedisTransport | LocalTransport |
|------------|----------------|----------------|
| Channel publish/receive | Yes | Yes |
| Scope routing (`GLOBAL`, `AREA`, `REGION`, `ROOM_WV`, etc.) | Yes | Yes |
| Moderation/penalties/filtering | Yes | Yes |
| Message history retrieval | Yes (durable stream) | Yes (memory ring buffer) |
| Report `incident_ref` IDs | Yes (stream IDs) | Yes (local IDs) |
| Cross-process / multi-instance propagation | Yes | No |
| Durable staff review queue | Yes | Optional (local memory or file mirror) |

### 8.5 Persistence Options in Local Mode

Base LocalTransport is memory-only. If stronger durability is needed without Redis, add optional append-only local logs:

- `data/system/channel_history.log` (or per-channel files)
- `data/system/channel_audit.log`

These can be replayed on startup to seed recent history and preserve staff review context. This is optional and should be configurable to avoid unnecessary disk I/O.

### 8.6 Implementation Notes by Phase

To align with the existing plan phases:

- **Phase 1:** Define transport interface and LocalTransport stubs before migrating channels.
- **Phase 2:** Implement RedisTransport + listener, then wire auto failover logic.
- **Phase 3/4:** Migrate channels without backend-specific code in `do_*` handlers.

### 8.7 Implementation Acceptance Criteria

Use this checklist to validate the fallback architecture in development and staging:

- [ ] **Backend isolation:** `do_*` channel handlers call only the generic channel service (no direct Redis calls).
- [ ] **Main-loop non-blocking:** No blocking socket/Redis operations are performed on the game loop thread.
- [ ] **Startup modes:** `channel_backend=redis|local|auto|legacy_iterative` all initialize correctly and log selected backend.
- [ ] **Redis outage failover:** In `auto` mode, forced Redis failure switches to LocalTransport within bounded retries and without server crash.
- [ ] **Redis recovery:** In `auto` mode, backend returns to Redis only after stable health checks (hysteresis/cooldown honored).
- [ ] **Single-process continuity:** Core channels (`gossip`, `ooc`, `say`, `tell`, `gtell`, `yell`, `churchtalk`) continue to function in LocalTransport mode.
- [ ] **Scope correctness:** Topic routing remains correct for `GLOBAL`, `AREA` (including `area_topic` override), `REGION`, `ROOM_WV`, `DIRECT_ENTITY`, `GROUP_ID`, and `CHURCH_ID`.
- [ ] **Multi-target correctness:** Channels using `route_targets` across multiple regions/areas/topic-groups deliver to the complete target set without duplicate delivery.
- [ ] **History retention:** Local ring buffers enforce per-channel `history.max_len` and `history.max_age_seconds` limits.
- [ ] **Message references:** Reports and moderation records still receive valid `incident_ref` IDs in LocalTransport mode.
- [ ] **Moderation parity:** Character/account penalties, mute/ban behavior, and escalation checks produce the same outcomes in Redis and Local modes.
- [ ] **Receiver parity:** `ignore`, word filters, and channel formatting execute identically across backends.
- [ ] **Script parity:** For room channels, message delivery occurs before speech triggers, matching current gameplay ordering.
- [ ] **No noisy flapping:** Repeated Redis instability does not cause rapid backend oscillation; transition logs are rate-limited and clear.
- [ ] **Optional durability behavior:** If local file mirroring is enabled, audit/history replay works after restart; if disabled, behavior is documented as memory-only.
- [ ] **Emergency path:** `legacy_iterative` mode remains available and operational as last-resort fallback.

### 8.8 Test Execution Matrix

The following matrix maps acceptance criteria to concrete validation steps. Exact command names may vary based on final implementation; keep these as canonical scenarios.

| Checklist Item | Scenario | Suggested Command / Action | Expected Result |
|----------------|----------|----------------------------|-----------------|
| Backend isolation | Build with pubsub enabled and inspect logs for transport use | Run server in normal mode; exercise `gossip`, `say`, `tell` | Channel flow goes through generic channel service, no direct Redis call paths in `do_*` commands |
| Startup modes | Start server once per backend mode | Set `channel_backend=redis`, `local`, `auto`, `legacy_iterative` and restart | Selected backend initializes successfully and logs backend name |
| Redis outage failover | Kill Redis during active chat traffic in `auto` mode | Start in `auto`, send channel messages, stop Redis service/socket | Backend switches to LocalTransport without crash; messages continue |
| Redis recovery | Restore Redis after outage in `auto` mode | Restart Redis; continue traffic | Backend returns to Redis only after stable checks/cooldown |
| Single-process continuity | Validate major channels in local mode | Start with `channel_backend=local`; test `gossip/ooc/say/tell/gtell/yell/churchtalk` | All channels deliver correctly in one server process |
| Scope correctness | Validate each scope with representative actors | Exercise `GLOBAL`, `AREA` (+ `area_topic` override), `REGION`, `ROOM_WV`, `DIRECT_ENTITY`, `GROUP_ID`, `CHURCH_ID` | Topic routing and recipients match scope rules |
| Multi-target routing | Validate continent/federation channels | Exercise `route_targets` spanning multiple regions and areas | All intended recipients get one delivery each |
| History retention | Overflow local history buffers and age windows | Send > `max_len` messages; advance time or simulate age expiry | Old entries trimmed by length/age policy |
| Message references | File reports in both backends | Report channel messages in Redis and local modes | `incident_ref` generated and retrievable in both modes |
| Moderation parity | Apply penalties and retest speaking/listening | Use `chanwarn/chanmute/chanban` + account-level penalties | Enforcement outcomes match between Redis and LocalTransport |
| Receiver parity | Validate ignore/filter/formatting with same message set | Configure `ignore` and word filters; send identical messages in both backends | Rendered output and suppressions are equivalent |
| Script parity | Test room speech trigger ordering | In scripted room, issue `say` with NPC/object triggers | Players see speech first, triggers execute afterward |
| No noisy flapping | Induce intermittent Redis failures | Toggle Redis availability rapidly while in `auto` | No rapid oscillation; transitions are controlled and clearly logged |
| Optional durability behavior | Enable local mirror and restart server | Turn on local audit/history mirror; send messages; restart | Replayed history/audit data appears as configured |
| Emergency path | Force both primary backends unavailable | Configure/break Redis and LocalTransport init; set or switch to `legacy_iterative` | Legacy iterative channel delivery remains functional |

#### Suggested Validation Order

1. Startup modes
2. Single-process continuity
3. Scope correctness
4. Receiver/moderation parity
5. History/message reference checks
6. Failover, recovery, and anti-flap behavior
7. Optional durability and emergency-path drills

This ensures each migrated channel automatically works in Redis and no-Redis environments.

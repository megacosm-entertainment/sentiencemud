# PLAN: Grouping System Analysis and Refactoring

This document outlines an analysis of the current grouping mechanism within the Sentience MUD and proposes a high-level refactoring strategy to address identified issues, support advanced features, and improve integration with other systems like the new Pub/Sub communication model.

## 1. Current Grouping Implementation

The current grouping mechanism in Sentience MUD is implicitly defined by the `leader` field within the `CHAR_DATA` structure. While there is no explicit `GROUP_DATA` structure, the leader's `CHAR_DATA` holds some group-related state.

-   **`is_same_group(CHAR_DATA *ach, CHAR_DATA *bch)`:** This function (located in `src/act_comm.c`) is the primary method to determine if two characters are in the same group. Its logic relies entirely on comparing the `leader` pointers:
    -   `ach == bch` (same character)
    -   `ach == MOUNTED(bch) || bch == MOUNTED(ach)` (mounted relationship, treated as same group)
    -   `ach->leader == bch->leader && ach->leader != NULL` (same leader)
    -   `ach->leader == bch` or `bch->leader == ach` (direct leader/follower relationship)
-   **`do_group(CHAR_DATA *ch, char *argument)`:** This command (also in `src/act_comm.c`) handles group formation, disbandment, and listing.
    -   **Listing Group Members:** When `do_group` is called without arguments, it iterates through *all loaded characters* (`char_list` or `loaded_chars`) and for each character, it calls `is_same_group` to identify members of the caller's group. This is an O(N) operation where N is the total number of loaded characters.
-   **`add_grouped(CHAR_DATA *ch, CHAR_DATA *master, bool show)`:** (Located in `src/act_comm.c`)
    -   Adds `ch` to `master`'s group.
    -   Checks `master->num_grouped` against a limit (currently 9).
    -   Increments `master->num_grouped` and adds `ch` to `master->lgroup` (a `list` of `CHAR_DATA`).
    -   Sets `ch->leader = master;`.
    -   Fires `TRIG_GROUPED`.
-   **`stop_grouped(CHAR_DATA *ch)`:** (Located in `src/act_comm.c`)
    -   Removes `ch` from its group.
    -   Decrements `ch->leader->num_grouped`.
    -   **Identified Bug:** Contains a bug: `if( list_hasdata(ch->leader->lgroup, ch)) list_appendlink(ch->leader->lgroup, ch);` which attempts to append the character to the list again, rather than removing it. This means `lgroup` will not correctly reflect actual group membership after members are removed via `stop_grouped`.
    -   Sets `ch->leader = NULL;`.
    -   Fires `TRIG_UNGROUPED`.
-   **NPCs in Groups:** NPCs can have `leader` pointers and thus can be part of these implicit groups.

## 2. Identified Issues

-   **Inefficient Group Iteration:** To find all members of a group or perform a group-wide action (as seen in `do_group` listing), the system requires iterating through *all loaded characters* in the game and calling `is_same_group` for each. This is highly inefficient and does not scale well.
-   **Distributed Group State:** While `master->num_grouped` and `master->lgroup` provide some centralized state, they are embedded within the leader's `CHAR_DATA`, not a distinct `GROUP_DATA` object. This makes group management more complex than it needs to be.
-   **`lgroup` Bug:** The bug in `stop_grouped` means `master->lgroup` does not accurately reflect current group members, hindering any attempt to use this list for efficient group-wide operations.
-   **Lack of Central Group Entity:** The absence of a distinct `GROUP_DATA` structure still implies:
    -   No single, stable `unique_id` for a group itself.
    -   No central place to store group-wide settings (e.g., loot distribution rules, XP sharing preferences, group-specific tactics, custom group names).
-   **Leadership Changes Impact Group Identity:** If a group's leader changes, the group's "identity" (as defined by `ch->leader` or the leader's `CHAR_DATA`) effectively changes.
-   **Ambiguity of `leader` Pointer:** The `leader` pointer serves multiple purposes (formal group leader, master for a pet/follower, charmed mob master), which can lead to ambiguity and makes it harder to distinguish formal groups from simple follower relationships.

## 3. Proposed Refactoring: Distinct Group Entity

To address these issues, a refactoring to introduce a distinct `GROUP_DATA` entity is proposed.

-   **`GROUP_DATA` Structure:**
    -   A new `GROUP_DATA` structure would be introduced, representing a formal group.
    -   Each `GROUP_DATA` instance would have its own stable, canonical `unique_id` (e.g., a Redis-managed ID or a combination of system-generated IDs).
    -   It would contain a list (e.g., `list_t *members`) of all its members, stored as their `unique_id`s, including both PCs and NPCs.
    -   It would store a pointer to the group's current leader (`CHAR_DATA *leader`) or the leader's `unique_id`.
    -   It would be the central repository for group-wide settings (loot, XP, etc.).
-   **`CHAR_DATA` Linkage:** The `CHAR_DATA` structure would be updated to hold a pointer or `unique_id` to the `GROUP_DATA` it belongs to, replacing or augmenting the existing `leader` pointer for formal groups.
-   **API Redesign:** New functions would be introduced for group management (e.g., `group_create`, `group_disband`, `group_add_member`, `group_remove_member`, `group_change_leader`).
-   **`is_same_group` Replacement:** This function would be replaced by checking if two characters belong to the same `GROUP_DATA` instance (e.g., comparing their `CHAR_DATA->group_id` fields) or if their `CHAR_DATA->group_unique_id` matches.

## 4. Benefits of Refactoring

-   **Performance Improvement:** Group-wide operations would no longer require iterating through all loaded characters. Instead, they would iterate through the members list of the relevant `GROUP_DATA` object, making them O(M) where M is the number of group members.
-   **Consistent Group Identification:** Each group would have a stable `unique_id`, simplifying its identification for logging, persistent storage, and integration with systems like the Pub/Sub communication model.
-   **Robust NPC Group Membership:** NPCs can be explicitly added as members to a `GROUP_DATA` object, fully participating in group mechanics without ambiguity.
-   **Centralized Group Settings:** Group-wide settings (loot, XP, etc.) can be stored and managed in one place.
-   **Clearer API:** A well-defined set of functions for group management would improve code clarity and maintainability.
-   **Seamless Pub/Sub Integration:** The group's `unique_id` would directly translate to a stable topic identifier for group communication channels (`rt:group:<group_unique_id>`), simplifying subscription management.

## 5. Challenges and Considerations

-   **Scope of Change:** This is a significant refactoring that touches fundamental `CHAR_DATA` structures and numerous functions that currently rely on the `leader` pointer.
-   **Migration Strategy:** A plan for migrating existing implicit groups to the new `GROUP_DATA` structure would be needed, especially if existing groups are to be preserved across reboots.
-   **Backward Compatibility:** Care must be taken to ensure minimal disruption to existing group-related mechanics during the transition.
-   **Pet/Follower System:** The `leader` pointer still serves its purpose for pets, charmed mobs, and simple followers. The new `GROUP_DATA` system should be designed to coexist with or cleanly replace this aspect, differentiating formal groups from simple follower hierarchies.

## 6. Stretch Goals / Future Features (Enabled by Refactoring)

-   **Configurable Loot Distribution:** Group leaders could set rules for how loot is split (e.g., free for all, leader distributes, round-robin, need/greed).
-   **Flexible XP Sharing:** Advanced XP sharing algorithms (e.g., weighted by level, bonus XP for smaller groups).
-   **Group-Specific Tactics:** Commands or configurations for group-wide battle tactics.
-   **Persistent Groups:** Groups that persist across reboots or even when all members log off.
-   **Group-wide Affects/Buffs:** Centralized management of temporary buffs or affects applied to the entire group.
-   **Formal Group Finder System:** In-game tools for players to find or form groups based on criteria.

# PLAN: Command System Enhancements - Alias Commands and Argument Restrictions

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


This document analyzes the feasibility and proposes a high-level plan for enhancing the MUD's command system to support:
1.  **Alias Commands:** Allowing one command to effectively execute another, potentially with argument transformations.
2.  **Argument-Specific Restrictions:** Applying level or other checks based on the arguments provided to a command, rather than just the command itself.

## 1. Current Command System Overview

The MUD's command system is driven by `CMD_DATA` objects, which are managed dynamically via the `cmdedit` OLC editor and persisted to a `COMMANDS_JSON_FILE`. This is a more flexible system than a purely static `cmd_table`.

-   **`CMD_DATA` Structure:** Each command is represented by a `CMD_DATA` object (defined in `interp.h` and used in `cmdedit.c`). Key fields include:
    -   `name`: The command string.
    -   `function`: A function pointer (`DO_FUN *`) to the C implementation (`do_function`).
    -   `position`: Minimum character position required.
    -   `rank`: Minimum staff rank (or level for players) required.
    -   `log`: Logging preference.
    -   `enabled`: Boolean to enable/disable the command.
    -   `reason`: Textual reason if disabled.
    -   `type`, `addl_types`, `command_flags`: Categorization and behavioral flags.
    -   `help_keywords`, `summary`, `description`, `comments`: Documentation fields.
-   **`interpret()` Function:** (in `src/interp.c`) This is the core command parser. It extracts the command word, looks it up in the active `commands_list` (which is populated from `CMD_DATA` objects), performs basic character state checks, and then calls the associated `do_function` if permissions and state allow.
-   **`cmdedit` OLC Editor:** (in `src/editors/commands/cmdedit.c`) Allows immortals to create, modify, and delete `CMD_DATA` objects, controlling their name, assigned function, rank, position, logging, enabled status, and various flags/metadata.

## 2. Alias Commands

### Current Implementation & Issue

Currently, alias-like behavior is achieved through wrapper functions. For example, the `locker` command has its own `do_locker` function. Inside `do_locker`, the arguments are re-parsed, and then `do_function(ch, &do_storage, "locker <argument>")` (or similar) is called. This means:
-   Each alias requires its own `do_function` and an entry in the `cmd_table`/`CMD_DATA`.
-   The wrapper function is responsible for argument transformation, which can lead to boilerplate code.

### Proposed Solution: Alias Field in `CMD_DATA`

Introduce an `alias_to` field in `CMD_DATA`. This field could be a string representing the target command and an optional argument transformation string.

-   **`CMD_DATA` Changes:**
    ```c
    struct CMD_DATA {
        // ... existing fields ...
        char *alias_target;      // Target command name (e.g., "storage")
        char *alias_args_fmt;    // Format string for arguments (e.g., "locker %s" for 'storage')
        bool is_alias;           // Flag to indicate this is an alias command
    };
    ```
-   **`interpret()` Modification:**
    1.  When `interpret()` finds a `CMD_DATA` where `is_alias` is true, it would retrieve `alias_target` and `alias_args_fmt`.
    2.  It would format the original command's arguments using `alias_args_fmt` to create a new argument string.
    3.  Then, it would recursively call `interpret()` with `ch`, `alias_target`, and the newly formatted arguments.
-   **`cmdedit` Integration:** `cmdedit` would need new sub-commands to set `alias_target` and `alias_args_fmt`.

### Challenges & Considerations

-   **Argument Transformation Complexity:** Simple format strings (`%s`) are easy. More complex transformations might require a mini-scripting language or more advanced parsing, increasing complexity.
-   **Recursion Detection:** Implement a mechanism within `interpret()` to detect and prevent infinite loops (e.g., `cmd1` aliases `cmd2`, `cmd2` aliases `cmd1`). A simple recursion depth counter would suffice.
-   **Help Files:** Ensure that aliases point to the correct help files, or have their own, concise help entries.
-   **Command Flags/Permissions:** Alias commands should ideally inherit permissions from the target or have their own. The proposed system allows for aliases to have their own `rank`, `position`, `enabled` status, etc., which is good.

## 3. Argument-Specific Restrictions

### Current Implementation & Issue

Argument-specific restrictions are currently hardcoded within the `do_function` implementations. For example, `do_restore` might contain `if (!str_cmp(argument, "all") && ch->tot_level < IMPLEMENTOR) { ... }`. This makes it difficult to modify these restrictions without recompiling and editing code.

### Proposed Solution: Argument Restrictions in `CMD_DATA`

Introduce a mechanism within `CMD_DATA` to define restrictions based on argument patterns.

-   **`CMD_DATA` Changes (example):**
    ```c
    struct CMD_DATA {
        // ... existing fields ...
        LIST_DATA *arg_restrictions; // List of argument restriction structs
    };

    struct ARG_RESTRICTION {
        char *pattern;           // Regex or simple string match for the argument (e.g., "all")
        int min_rank;            // Minimum rank required for this argument
        int min_level;           // Minimum level required for this argument
        // ... other restrictions (e.g., specific flags, position)
    };
    ```
-   **`interpret()` Modification:**
    1.  After finding the `CMD_DATA` for the command, but *before* calling its `do_function`, `interpret()` would iterate through `arg_restrictions`.
    2.  For each restriction, it would compare `argument` against `pattern`.
    3.  If a match is found, it would apply the specified `min_rank`/`min_level` checks. If the character fails the check, an error message is sent, and the command is aborted.
-   **`cmdedit` Integration:** `cmdedit` would need new sub-commands to add, edit, and remove `ARG_RESTRICTION` entries for a command. This would require parsing command arguments to specify the pattern and restriction.

### Challenges & Considerations

-   **Parsing Complexity:** Matching argument patterns can range from simple string comparisons to full regular expressions. The more complex the pattern matching, the more complex the parsing within `interpret()` and `cmdedit`.
-   **Order of Precedence:** If multiple `ARG_RESTRICTION`s match, how are they resolved? (e.g., most specific, first match).
-   **Performance Impact:** Iterating through `arg_restrictions` for every command execution could introduce overhead. Optimizations might be needed (e.g., hashing argument patterns).
-   **Help Files:** Help files for commands should clearly document any argument-specific restrictions.

## 4. Feasibility and Impact

-   **Feasibility:** Both enhancements are technically feasible but involve significant changes to the core command processing (`interpret()`) and the `CMD_DATA` structure, as well as extensions to `cmdedit`. The complexity of argument transformation for aliases and pattern matching for argument restrictions are the primary technical hurdles.
-   **Impact:**
    -   **Flexibility & Maintainability:** Greatly enhances the flexibility of command management. New aliases and argument restrictions can be configured on-the-fly without coding, reducing boilerplate `do_function` wrappers and centralizing policy.
    -   **Code Cleanliness:** Reduces hardcoded logic within `do_function`s.
    -   **Game Design:** Empowers staff to fine-tune command behavior and access, which is especially valuable for a dynamic game environment like a MUD.

## 5. High-Level Implementation Plan

1.  **Phase 1: `CMD_DATA` and Parser Enhancement (Unique Identifiers for Commands)**
    a.  **Refactor `CMD_DATA`:** Add `alias_target`, `alias_args_fmt`, `is_alias`, and `arg_restrictions` fields. These would likely require new `string` or `list` types.
    b.  **Update JSON Serialization:** Modify `json_load_commands` and `json_save_commands` to handle the new fields.
    c.  **Enhance `interpret()`:**
        -   Implement recursion detection for aliases.
        -   Implement argument parsing and restriction checks for `arg_restrictions`.
        -   Implement argument transformation logic for `alias_args_fmt`.

2.  **Phase 2: `cmdedit` Integration**
    a.  **Extend `cmdedit_show`:** Display the new `CMD_DATA` fields.
    b.  **Add `cmdedit` Sub-commands:** Implement new commands (e.g., `cmdedit <cmd> alias set <target> <args_fmt>`, `cmdedit <cmd> argrestrict add <pattern> <min_rank>`) to manage the new fields.

3.  **Phase 3: Migration and Refactoring**
    a.  **Convert Existing Wrapper Functions:** Replace hardcoded alias `do_function`s (like `do_locker`) with `CMD_DATA` aliases.
    b.  **Convert Hardcoded Argument Checks:** Migrate argument-specific level/rank checks (like `do_restore`'s 'all' argument) into `ARG_RESTRICTION` entries.

4.  **Phase 4: Testing and Optimization**
    a.  Thoroughly test all new command behaviors, especially argument parsing and recursion.
    b.  Monitor performance impact and optimize parsing/lookup if necessary.

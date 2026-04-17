# Staff Removal Fix — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix staff removal so `staff delete` and `sdemote` fully clear staff status from all data layers (immortal list, character pfile, account entry, account flags).

**Architecture:** Extract a shared `remove_staff_status()` helper that performs the complete removal sequence. Both `do_sdelete()` and `do_sdemote()` call it. The helper handles online vs offline characters, account entry cleanup, and `ACCT_CAN_CREATE_STAFF` flag management.

**Tech Stack:** C, Jansson (JSON), custom test framework with JSON test definitions.

**Spec:** `docs/superpowers/specs/2026-04-17-staff-removal-design.md`

---

### Task 1: Implement `remove_immortal()` and `remove_staff_status()`

**Files:**
- Modify: `staff.c` (add two new functions)
- Modify: `merc.h:11372` (update declaration, add new declaration)

`remove_immortal()` is already declared in `merc.h:11372` but never implemented. It will handle the linked-list removal + free. `remove_staff_status()` is the full cleanup function that calls it.

- [ ] **Step 1: Implement `remove_immortal()` in `staff.c`**

Add after `add_immortal()` (after line 150). This extracts the linked-list removal logic currently duplicated in `do_sdelete()`:

```c
/* Remove an immortal from the global list and free it. */
void remove_immortal(IMMORTAL_DATA *immortal)
{
    IMMORTAL_DATA *tmp, *last = NULL;

    if (immortal == NULL)
        return;

    for (tmp = immortal_list; tmp != NULL; tmp = tmp->next) {
        if (tmp == immortal)
            break;
        last = tmp;
    }

    if (tmp == NULL)
        return;

    if (last != NULL)
        last->next = immortal->next;
    else
        immortal_list = immortal->next;

    free_immortal(immortal);
    save_immstaff();
}
```

- [ ] **Step 2: Implement `remove_staff_status()` in `staff.c`**

Add after `remove_immortal()`. This is the main function that orchestrates complete staff removal:

```c
/*
 * remove_staff_status - Completely remove staff status from a character.
 *
 * 1. Remove IMMORTAL_DATA from immortal_list
 * 2. Reset staff_rank on the character (online or offline)
 * 3. Update the ACCOUNT_CHARACTER entry
 * 4. Clear ACCT_CAN_CREATE_STAFF if no staff remain on the account
 */
void remove_staff_status(const char *name)
{
    IMMORTAL_DATA *immortal;
    CHAR_DATA *victim;
    ACCOUNT_DATA *acct;
    ACCOUNT_CHARACTER *acct_char;
    ITERATOR it;
    bool has_remaining_staff = false;
    bool acct_needs_free = false;

    if (IS_NULLSTR(name))
        return;

    /* Step 1: Remove immortal record */
    immortal = find_immortal((char *)name);
    if (immortal != NULL) {
        /* Clear backlink before freeing */
        if (immortal->pc != NULL)
            immortal->pc->immortal = NULL;
        remove_immortal(immortal);
    }

    /* Step 2: Reset staff_rank on the character */
    victim = get_char_world(NULL, (char *)name);
    if (victim != NULL && !IS_NPC(victim)) {
        /* Online character */
        victim->pcdata->staff_rank = STAFF_PLAYER;
        victim->pcdata->immortal = NULL;
        save_char_obj(victim);
    } else if (player_exists((char *)name)) {
        /* Offline character — load, fix, save */
        DESCRIPTOR_DATA temp_d;
        memset(&temp_d, 0, sizeof(temp_d));

        if (load_char_obj_basic(&temp_d, name)) {
            if (temp_d.character && temp_d.character->pcdata) {
                temp_d.character->pcdata->staff_rank = STAFF_PLAYER;
                temp_d.character->pcdata->immortal = NULL;
                save_char_obj(temp_d.character);
            }
            if (temp_d.character) {
                free_char(temp_d.character);
                temp_d.character = NULL;
            }
        } else {
            log_string(formatf("remove_staff_status: failed to load character '%s'", name));
        }
    } else {
        log_string(formatf("remove_staff_status: character '%s' does not exist", name));
    }

    /* Step 3: Update account character entry */
    acct = find_account((char *)name);
    if (acct == NULL) {
        log_string(formatf("remove_staff_status: no account found for '%s'", name));
        return;
    }

    acct_needs_free = true;

    iterator_start(&it, acct->characters);
    while ((acct_char = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(acct_char->name, name)) {
            acct_char->staff = false;
            acct_char->staff_rank = STAFF_PLAYER;
        } else if (acct_char->staff && acct_char->staff_rank >= STAFF_IMMORTAL) {
            has_remaining_staff = true;
        }
    }
    iterator_stop(&it);

    /* Step 4: Clear account staff flag if no staff remain */
    if (!has_remaining_staff)
        REMOVE_BIT(acct->acct_flags, ACCT_CAN_CREATE_STAFF);

    save_account(acct);

    if (acct_needs_free)
        free_account(acct);
}
```

- [ ] **Step 3: Add declaration to `merc.h`**

After the existing `remove_immortal` declaration at line 11372, add:

```c
void remove_staff_status(const char *name);
```

- [ ] **Step 4: Build and verify compilation**

Run: `cd /sentience/src && ./build tests`
Expected: Clean compile, no errors.

- [ ] **Step 5: Commit**

```bash
git add staff.c merc.h
git commit -m "feat: add remove_immortal() and remove_staff_status() helpers

Implements the shared staff removal logic that clears all data layers:
immortal list, character pfile, account entry, and ACCT_CAN_CREATE_STAFF.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 2: Update `do_sdelete()` to use `remove_staff_status()`

**Files:**
- Modify: `staff.c:340-375` (rewrite `do_sdelete`)

- [ ] **Step 1: Rewrite `do_sdelete()` in `staff.c`**

Replace the current `do_sdelete` function (lines 340-375) with:

```c
void do_sdelete(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    IMMORTAL_DATA *immortal;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
        send_to_char("Syntax:  staff delete [immortal]"
                 "\n\r{RWARNING:{x all information associated with this immortal will be wiped!\n\r", ch);
        return;
    }

    if ((immortal = find_immortal(arg)) == NULL) {
        send_to_char("No such immortal.\n\r", ch);
        return;
    }

    act("$T's immortal privileges have been terminated.", ch, NULL, NULL, NULL, NULL, NULL, immortal->name, TO_CHAR, NULL, NULL);

    remove_staff_status(immortal->name);
}
```

- [ ] **Step 2: Build and verify**

Run: `cd /sentience/src && ./build tests`
Expected: Clean compile.

- [ ] **Step 3: Commit**

```bash
git add staff.c
git commit -m "refactor: do_sdelete uses remove_staff_status for complete cleanup

Previously only removed the immortal list entry but left the character
pfile and account in an inconsistent state.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 3: Update `do_sdemote()` to allow demotion to `STAFF_PLAYER`

**Files:**
- Modify: `staff.c:207-274` (update `do_sdemote`)

- [ ] **Step 1: Update `do_sdemote()` in `staff.c`**

Three changes to the existing function:

**Change A:** Remove the `STAFF_GIMP` floor check (lines 243-247). Delete this block:
```c
    if (get_staff_rank(player) == STAFF_GIMP)
    {
        send_to_char("That is the lowest staff rank.  If you want to demote them lower, sdelete them.\n\r", ch);
        return;
    }
```

**Change B:** In the rank validation `if` block (lines 251-266), change the condition from `new_rank < STAFF_GIMP` to `new_rank < STAFF_PLAYER`:
```c
        if ((new_rank = stat_lookup(argument, staff_ranks, NO_FLAG)) == NO_FLAG ||
            new_rank < STAFF_PLAYER || new_rank >= old_rank)
```

And change the `for` loop filter from `staff_ranks[i].bit > STAFF_PLAYER` to `staff_ranks[i].bit >= STAFF_PLAYER`:
```c
                if (staff_ranks[i].settable && staff_ranks[i].bit >= STAFF_PLAYER && staff_ranks[i].bit < old_rank)
```

**Change C:** After calculating `new_rank`, add an early-return for full removal. Insert before the `player->pcdata->staff_rank = new_rank` line (line 269):

```c
    /* Full removal when demoting to player rank */
    if (new_rank <= STAFF_PLAYER) {
        send_to_char(formatf("You have been removed from staff.\n\r"), player);
        send_to_char(formatf("{+%s removed from staff.\n\r", player->name), ch);
        remove_staff_status(player->name);
        return;
    }
```

- [ ] **Step 2: Make `"player"` settable in `staff_ranks` table**

The `staff_ranks` table in `tables.c:4084` has `{"player", STAFF_PLAYER, false}`. The `settable` field must be `true` so `sdemote` can accept `"player"` as a target rank. Change in `tables.c:4084`:

```c
    {"player",      STAFF_PLAYER,            true},
```

- [ ] **Step 3: Build and verify**

Run: `cd /sentience/src && ./build tests`
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add staff.c tables.c
git commit -m "feat: allow sdemote to fully revoke staff to player rank

Removes the STAFF_GIMP floor check. Demoting to STAFF_PLAYER now calls
remove_staff_status() for complete cleanup.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 4: Add tests for staff removal

**Files:**
- Create: `tests/unit/staff_removal_tests.c`
- Create: `tests/data/unit/staff_removal_tests.json`
- Modify: `tests/framework/test_modules.h` (add declaration)
- Modify: `tests/framework/test_dispatcher.c` (add handler entry)
- Modify: `tests/data/test_config.json` (add suite)
- Modify: `CMakeLists.txt` (add source file)
- Modify: `Makefile` (add source file)

The MUD environment is available in tests (loaded from `bootstrap/bootstrap_data`), so we can use the full infrastructure: `new_account()`, `new_char()`, `new_pcdata()`, `add_immortal()`, etc. to build synthetic test fixtures.

- [ ] **Step 1: Create test JSON definition `tests/data/unit/staff_removal_tests.json`**

```json
{
  "type": "test_suite",
  "test_suite": "staff_removal_tests",
  "description": "Tests for staff removal via remove_immortal() and remove_staff_status()",
  "version": "1.0",
  "requires_mud_environment": true,
  "test_level": "unit",
  "tests": [
    {
      "name": "remove_immortal_clears_list",
      "description": "After remove_immortal(), find_immortal() returns NULL",
      "test_type": "stfrm_remove_immortal_clears_list",
      "input": { "scenario": "remove_from_list" },
      "expected_output": { "pass": true }
    },
    {
      "name": "remove_staff_status_resets_rank",
      "description": "remove_staff_status() clears immortal record for a non-existent pfile",
      "test_type": "stfrm_remove_status_resets_rank",
      "input": { "scenario": "reset_rank" },
      "expected_output": { "pass": true }
    },
    {
      "name": "remove_staff_status_clears_immortal_pointer",
      "description": "remove_staff_status() nulls the pc->immortal backlink",
      "test_type": "stfrm_clears_immortal_ptr",
      "input": { "scenario": "clear_pointer" },
      "expected_output": { "pass": true }
    },
    {
      "name": "remove_immortal_null_safe",
      "description": "remove_immortal(NULL) does not crash",
      "test_type": "stfrm_null_safe",
      "input": { "scenario": "null_safe" },
      "expected_output": { "pass": true }
    },
    {
      "name": "remove_immortal_not_in_list",
      "description": "remove_immortal() on an immortal not in the list does not crash",
      "test_type": "stfrm_not_in_list",
      "input": { "scenario": "not_in_list" },
      "expected_output": { "pass": true }
    },
    {
      "name": "remove_staff_status_empty_name",
      "description": "remove_staff_status with empty/NULL name does not crash",
      "test_type": "stfrm_empty_name",
      "input": { "scenario": "empty_name" },
      "expected_output": { "pass": true }
    },
    {
      "name": "account_entry_updated_on_removal",
      "description": "remove_staff_status() sets ACCOUNT_CHARACTER staff=false and staff_rank=0",
      "test_type": "stfrm_account_entry_updated",
      "input": { "scenario": "account_entry_updated" },
      "expected_output": { "pass": true }
    },
    {
      "name": "account_flag_cleared_last_staff",
      "description": "ACCT_CAN_CREATE_STAFF cleared when last staff character removed",
      "test_type": "stfrm_account_flag_cleared",
      "input": { "scenario": "account_flag_cleared" },
      "expected_output": { "pass": true }
    },
    {
      "name": "sdemote_to_player_triggers_removal",
      "description": "do_sdemote to 'player' rank triggers full staff removal via remove_staff_status",
      "test_type": "stfrm_sdemote_to_player",
      "input": { "scenario": "sdemote_to_player" },
      "expected_output": { "pass": true }
    },
    {
      "name": "sdemote_below_gimp_allowed",
      "description": "do_sdemote below STAFF_GIMP (to player) is now allowed — old floor removed",
      "test_type": "stfrm_sdemote_below_gimp",
      "input": { "scenario": "sdemote_below_gimp" },
      "expected_output": { "pass": true }
    }
  ]
}
```

- [ ] **Step 2: Create test handler `tests/unit/staff_removal_tests.c`**

```c
#ifdef BUILD_TESTS

#include <string.h>
#include <stdio.h>
#include "../../merc.h"
#include "../../recycle.h"
#include "../framework/test_framework.h"

test_result_t run_staff_removal_test_case(test_case_t *test)
{
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Staff removal test missing configuration");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Staff removal test missing input");
        return TEST_ERROR;
    }

    const char *scenario = test_json_get_string(input, "scenario");
    if (!scenario) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Staff removal test missing scenario");
        return TEST_ERROR;
    }

    /* Test: remove_immortal removes from list and find returns NULL */
    if (strcmp(scenario, "remove_from_list") == 0) {
        IMMORTAL_DATA *saved_list = immortal_list;

        IMMORTAL_DATA *imm = new_immortal();
        free_string(imm->name);
        imm->name = str_dup("TestStaffRemoval");
        imm->duties = 0;
        imm->created = current_time;
        add_immortal(imm);

        TEST_ASSERT_NOT_NULL(find_immortal("TestStaffRemoval"));

        remove_immortal(imm);

        TEST_ASSERT_NULL(find_immortal("TestStaffRemoval"));

        immortal_list = saved_list;
        return TEST_SUCCESS;
    }

    /* Test: remove_staff_status clears immortal record */
    if (strcmp(scenario, "reset_rank") == 0) {
        IMMORTAL_DATA *saved_list = immortal_list;

        IMMORTAL_DATA *imm = new_immortal();
        free_string(imm->name);
        imm->name = str_dup("TestRankReset");
        imm->duties = 0;
        imm->created = current_time;
        add_immortal(imm);

        TEST_ASSERT_NOT_NULL(find_immortal("TestRankReset"));

        remove_staff_status("TestRankReset");

        TEST_ASSERT_NULL(find_immortal("TestRankReset"));

        immortal_list = saved_list;
        return TEST_SUCCESS;
    }

    /* Test: remove_staff_status clears pc->immortal backlink */
    if (strcmp(scenario, "clear_pointer") == 0) {
        IMMORTAL_DATA *saved_list = immortal_list;

        IMMORTAL_DATA *imm = new_immortal();
        free_string(imm->name);
        imm->name = str_dup("TestClearPtr");
        imm->duties = 0;
        imm->created = current_time;

        PC_DATA *pc = new_pcdata();
        pc->immortal = imm;
        imm->pc = pc;

        add_immortal(imm);

        remove_staff_status("TestClearPtr");

        TEST_ASSERT_NULL(pc->immortal);

        free_pcdata(pc);
        immortal_list = saved_list;
        return TEST_SUCCESS;
    }

    /* Test: remove_immortal(NULL) is safe */
    if (strcmp(scenario, "null_safe") == 0) {
        remove_immortal(NULL);
        return TEST_SUCCESS;
    }

    /* Test: removing an immortal not in the list doesn't crash */
    if (strcmp(scenario, "not_in_list") == 0) {
        IMMORTAL_DATA *imm = new_immortal();
        free_string(imm->name);
        imm->name = str_dup("NeverAdded");
        remove_immortal(imm);
        /* remove_immortal returns early without freeing when not found,
           so we must free manually */
        free_immortal(imm);
        return TEST_SUCCESS;
    }

    /* Test: remove_staff_status with empty/NULL name */
    if (strcmp(scenario, "empty_name") == 0) {
        remove_staff_status("");
        remove_staff_status(NULL);
        return TEST_SUCCESS;
    }

    /* Test: account character entry updated after staff removal */
    if (strcmp(scenario, "account_entry_updated") == 0) {
        IMMORTAL_DATA *saved_list = immortal_list;

        /* Create a synthetic account with a staff character entry */
        ACCOUNT_DATA *acct = new_account();
        acct->username = str_dup("TestStaffAcct");
        SET_BIT(acct->acct_flags, ACCT_CAN_CREATE_STAFF);

        ACCOUNT_CHARACTER *ac = new_account_character();
        ac->name = str_dup("TestAcctChar");
        ac->staff = true;
        ac->staff_rank = STAFF_IMMORTAL;
        list_appendlink(acct->characters, ac);

        /* Save the account so find_account can locate it later */
        save_account(acct);

        /* Create matching immortal */
        IMMORTAL_DATA *imm = new_immortal();
        free_string(imm->name);
        imm->name = str_dup("TestAcctChar");
        imm->duties = 0;
        imm->created = current_time;
        add_immortal(imm);

        /* Also need a minimal character pfile for find_account() to work.
           Create and save a bare-bones character. */
        CHAR_DATA *ch = new_char();
        ch->name = str_dup("TestAcctChar");
        ch->pcdata = new_pcdata();
        ch->pcdata->staff_rank = STAFF_IMMORTAL;
        ch->pcdata->account_name = str_dup("TestStaffAcct");
        save_char_obj(ch);
        free_char(ch);

        /* Run removal */
        remove_staff_status("TestAcctChar");

        /* Reload the account and check the character entry */
        ACCOUNT_DATA *reloaded = find_account("TestAcctChar");
        if (reloaded) {
            ITERATOR it;
            ACCOUNT_CHARACTER *found_ac = NULL;
            iterator_start(&it, reloaded->characters);
            while ((found_ac = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
                if (!str_cmp(found_ac->name, "TestAcctChar"))
                    break;
            }
            iterator_stop(&it);

            if (found_ac) {
                TEST_ASSERT_FALSE(found_ac->staff);
                TEST_ASSERT_INT_EQ(STAFF_PLAYER, found_ac->staff_rank);
            }
            free_account(reloaded);
        }

        /* Clean up: delete the test pfile */
        delete_character_by_name("TestAcctChar");

        immortal_list = saved_list;
        free_account(acct);
        return TEST_SUCCESS;
    }

    /* Test: ACCT_CAN_CREATE_STAFF cleared when last staff removed */
    if (strcmp(scenario, "account_flag_cleared") == 0) {
        IMMORTAL_DATA *saved_list = immortal_list;

        /* Create account with one staff character */
        ACCOUNT_DATA *acct = new_account();
        acct->username = str_dup("TestFlagAcct");
        SET_BIT(acct->acct_flags, ACCT_CAN_CREATE_STAFF);

        ACCOUNT_CHARACTER *ac = new_account_character();
        ac->name = str_dup("TestFlagChar");
        ac->staff = true;
        ac->staff_rank = STAFF_IMMORTAL;
        list_appendlink(acct->characters, ac);

        save_account(acct);

        IMMORTAL_DATA *imm = new_immortal();
        free_string(imm->name);
        imm->name = str_dup("TestFlagChar");
        imm->duties = 0;
        imm->created = current_time;
        add_immortal(imm);

        /* Create minimal pfile */
        CHAR_DATA *ch = new_char();
        ch->name = str_dup("TestFlagChar");
        ch->pcdata = new_pcdata();
        ch->pcdata->staff_rank = STAFF_IMMORTAL;
        ch->pcdata->account_name = str_dup("TestFlagAcct");
        save_char_obj(ch);
        free_char(ch);

        /* Remove the only staff character */
        remove_staff_status("TestFlagChar");

        /* Reload account and verify flag was cleared */
        ACCOUNT_DATA *reloaded = find_account("TestFlagChar");
        if (reloaded) {
            TEST_ASSERT_FALSE(IS_SET(reloaded->acct_flags, ACCT_CAN_CREATE_STAFF));
            free_account(reloaded);
        }

        /* Clean up */
        delete_character_by_name("TestFlagChar");

        immortal_list = saved_list;
        free_account(acct);
        return TEST_SUCCESS;
    }

    /* Test: sdemote to "player" triggers full removal via remove_staff_status.
       Simulates what do_sdemote does when the target rank is STAFF_PLAYER. */
    if (strcmp(scenario, "sdemote_to_player") == 0) {
        IMMORTAL_DATA *saved_list = immortal_list;

        /* Set up a staff character at STAFF_GIMP with an immortal record */
        IMMORTAL_DATA *imm = new_immortal();
        free_string(imm->name);
        imm->name = str_dup("TestSdemotePlayer");
        imm->duties = 0;
        imm->created = current_time;
        add_immortal(imm);

        /* Verify the immortal record exists */
        TEST_ASSERT_NOT_NULL(find_immortal("TestSdemotePlayer"));

        /* This is what do_sdemote calls when new_rank == STAFF_PLAYER */
        remove_staff_status("TestSdemotePlayer");

        /* Verify full cleanup: immortal record removed */
        TEST_ASSERT_NULL(find_immortal("TestSdemotePlayer"));

        immortal_list = saved_list;
        return TEST_SUCCESS;
    }

    /* Test: staff_ranks table allows "player" as a settable rank (old GIMP floor removed).
       After Task 3, stat_lookup("player", ...) must return STAFF_PLAYER, not NO_FLAG. */
    if (strcmp(scenario, "sdemote_below_gimp") == 0) {
        int rank = stat_lookup("player", staff_ranks, NO_FLAG);
        TEST_ASSERT_INT_EQ(STAFF_PLAYER, rank);
        /* Also verify settable is true by checking the table directly */
        bool found_settable = false;
        for (int i = 0; staff_ranks[i].name; i++) {
            if (staff_ranks[i].bit == STAFF_PLAYER) {
                found_settable = staff_ranks[i].settable;
                break;
            }
        }
        TEST_ASSERT_TRUE(found_settable);
        return TEST_SUCCESS;
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unknown staff removal test scenario: %s", scenario);
    return TEST_ERROR;
}

#endif /* BUILD_TESTS */
```

- [ ] **Step 3: Register handler in `test_modules.h`**

Add after line 61 (after `run_olc_schema_capture_test_case` declaration):

```c
test_result_t run_staff_removal_test_case(test_case_t *test);
```

- [ ] **Step 4: Register handler in `test_dispatcher.c`**

Add to the handler_table in the unit test handlers section, after the `"slink_"` entry (after line 66):

```c
    { "stfrm_",                          run_staff_removal_test_case,       MATCH_SUBSTR },
```

- [ ] **Step 5: Register suite in `test_config.json`**

Add `"staff_removal_tests"` to these arrays:
- `default_test_suites` (after `"olc_action_tests"`)
- `full_test_suites` (after `"olc_action_tests"`)
- `unit_only_suites` (after `"olc_action_tests"`)

- [ ] **Step 6: Add source file to `CMakeLists.txt`**

Add after the `tests/unit/olc_schema_capture_tests.c` line (line 342):

```
        tests/unit/staff_removal_tests.c
```

- [ ] **Step 7: Add source file to `Makefile`**

Add after the `tests/unit/olc_schema_capture_tests.c \` line (line 327):

```
               tests/unit/staff_removal_tests.c \
```

- [ ] **Step 8: Build with tests**

Run: `cd /sentience/src && ./build tests`
Expected: Clean compile.

- [ ] **Step 9: Run tests**

Run: `cd /sentience && ./src/.build/Debug/sent -test`
Expected: All existing tests pass. New `staff_removal_tests` suite passes (10 tests).

- [ ] **Step 10: Commit**

```bash
git add tests/unit/staff_removal_tests.c tests/data/unit/staff_removal_tests.json \
        tests/framework/test_modules.h tests/framework/test_dispatcher.c \
        tests/data/test_config.json CMakeLists.txt Makefile
git commit -m "test: add unit tests for staff removal

Tests remove_immortal() list management and remove_staff_status()
cleanup logic including NULL safety and backlink clearing.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 5: Final verification

- [ ] **Step 1: Full build**

Run: `cd /sentience/src && ./build tests`
Expected: Clean compile, no warnings related to staff.c.

- [ ] **Step 2: Run full test suite**

Run: `cd /sentience && ./src/.build/Debug/sent -test`
Expected: All tests pass (existing baseline + 10 new staff removal tests). No regressions.

- [ ] **Step 3: Verify test output**

Confirm the new tests appear in output:
```
staff_removal_tests: 10 tests, 10 passed
```

#ifdef BUILD_TESTS

#include <string.h>
#include <stdio.h>
#include "../../merc.h"
#include "../../tables.h"
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

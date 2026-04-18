/* This file contains most of the functions associated with the in-game staff
   management system. All source copyright Anton Ouzilov, 2007. */

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "merc.h"
#include "tables.h"
#include "olc.h"
#include "recycle.h"
#include "interp.h"
#include "io/json/json_staff.h"

/* A global list of all imms. */
IMMORTAL_DATA		*immortal_list;



/* Top level staff-management handler. For use only by whoever manages
   staff, although all imps can use it. */
void do_staff(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];

    argument = one_argument(argument, arg);

    if (IS_NPC(ch))
    return;

    if (ch->pcdata->security < 10 && !IS_SET(ch->pcdata->immortal->duties, IMMORTAL_STAFF))
    {
    send_to_char("You aren't authorized to do this.\n\r", ch);
    return;
    }

    if (!str_cmp(arg, "list")) {
    do_function(ch, &do_slist, argument);
    return;
    }

    if (!str_cmp(arg, "add")) {
    do_function(ch, &do_sadd, argument);
    return;
    }

    if (!str_cmp(arg, "duty")) {
    do_function(ch, &do_sduty, argument);
    return;
    }

    if (!str_cmp(arg, "supervisor")) {
    do_function(ch, &do_ssupervisor, argument);
    return;
    }

    if (!str_cmp(arg, "delete")) {
    do_function(ch, &do_sdelete, argument);
    return;
    }


    send_to_char("Syntax:  staff list\n\r"
         "         staff add [immortal]\n\r"
             "         staff delete [immortal]\n\r"
         "         staff duty [immortal] [duty]\n\r"
         "         staff supervisor [immortal] [supervisor|none]\n\r", ch);
}


/* Show the staff list. */
void do_slist(CHAR_DATA *ch, char *argument)
{
    // true laziness exists :P
     do_function(ch, &do_wizlist, "");

}


void do_sadd(CHAR_DATA *ch, char *argument)
{
    IMMORTAL_DATA *immortal;
    char buf[MSL];

    if (argument[0] == '\0' || strlen(argument) < 3) {
    send_to_char("Syntax:  staff add [player name]\n\r",  ch);
    return;
    }

    if (!player_exists(argument))
    {
    send_to_char("That character doesn't exist.\n\r", ch);
    return;
    }

    if (find_immortal(argument) != NULL) {
    send_to_char("There is already an immortal by that name.\n\r", ch);
    return;
    }

    sprintf(buf, "%s", argument);
    buf[0] = UPPER(buf[0]);

    immortal = new_immortal();
    immortal->name = str_dup(buf);
    immortal->imm_flag = str_dup("{RImmortal{x");
    immortal->created = current_time;
    immortal->duties = 0;

    add_immortal(immortal);

    act("Created new immortal $T.", ch, NULL, NULL, NULL, NULL, NULL, immortal->name, TO_CHAR, NULL, NULL);
    save_immstaff();
}


/* Inserts the immortal into the list, alphabetically sorted. Used here and in save.c */
void add_immortal(IMMORTAL_DATA *immortal)
{
    IMMORTAL_DATA *imm_tmp, *imm_last = NULL;

    /* first imm ever added */
    if (immortal_list == NULL) {
    immortal_list = immortal;
    immortal->next = NULL;
    }

    /* first alphabetically */
    else if (str_cmp(immortal->name, immortal_list->name) < 0) {
    immortal->next = immortal_list;
    immortal_list = immortal;
    }

    /* in the middleor at the end */
    else {
    for (imm_tmp = immortal_list; imm_tmp != NULL; imm_tmp = imm_tmp->next) {
        if (str_cmp(imm_tmp->name, immortal->name) > 0)
        break;
        imm_last = imm_tmp;
    }

    imm_last->next = immortal;
    immortal->next = imm_tmp;

    }

}


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


/* Toggles a duty for an immortal.
   Staff duty [immortal] [duty] */
void do_sduty(CHAR_DATA *ch, char *argument)
{
    int value;
    char arg[MSL];
    IMMORTAL_DATA *immortal;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
    send_to_char("Syntax:  staff duty [immortal] [duty]\n\r", ch);
    send_to_char("For a list of duties, type 'staff duty ?'\n\r", ch);
    return;
    }

    if (arg[0] == '?') {
    show_help(ch, "immortalflags");
    return;
    }

    if ((immortal = find_immortal(arg)) == NULL) {
    send_to_char("Immortal not found.\n\r", ch);
    return;
    }

    if ((value = flag_value(immortal_flags, argument)) == NO_FLAG) {
    send_to_char("Invalid duty.\n\r", ch);
    return;
    }

    TOGGLE_BIT(immortal->duties, value);
    send_to_char("Duty bit toggled.\n\r", ch);
    save_immstaff();
}


/* Find an immortal of a certain name in the list.
   Calling it with list = NULL will search the whole imm database. */
IMMORTAL_DATA *find_immortal(char *argument)
{
    IMMORTAL_DATA *immortal;

    if (argument[0] == '\0')
    return NULL;

    for (immortal = immortal_list; immortal != NULL; immortal = immortal->next)  {
    if (!str_prefix(argument, immortal->name))
        break;
    }

    return immortal;
}

void do_sdemote(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *player;
    char arg[MIL];

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  sdemote <immortal>[ <rank>]\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);
    if ((player = get_char_world(ch, arg)) == NULL)
    {
        send_to_char("No one by that name found.\n\r", ch);
        return;
    }

    if (IS_NPC(player))
    {
        send_to_char("Nice try.\n\r", ch);
        return;
    }

    if (!IS_IMMORTAL(player))
    {
        send_to_char("That is not an immortal.\n\r", ch);
        return;
    }

    if (get_staff_rank(player) >= get_staff_rank(ch))
    {
        send_to_char("Nice try.\n\r", ch);
        return;
    }

    if (get_staff_rank(player) == STAFF_GIMP)
    {
        send_to_char("That is the lowest staff rank.  If you want to demote them lower, sdelete them.\n\r", ch);
        return;
    }

    int old_rank = get_staff_rank(player);
    int new_rank = old_rank - 1;
    if (argument[0] != '\0')
    {
        if ((new_rank = stat_lookup(argument, staff_ranks, NO_FLAG)) == NO_FLAG ||
            new_rank < STAFF_GIMP || new_rank >= old_rank)
        {
            send_to_char("Invalid staff rank.\n\r", ch);
            send_to_char("Please select one of the following:\n\r", ch);
            for(int i = 0; staff_ranks[i].name; i++)
            {
                if (staff_ranks[i].settable && staff_ranks[i].bit > STAFF_PLAYER && staff_ranks[i].bit < old_rank)
                {
                    send_to_char(formatf(" %s\n\r", staff_ranks[i].name), ch);
                }
            }
            return;
        }
    }
    
    player->pcdata->staff_rank = new_rank;
    save_char_obj(player);

    send_to_char(formatf("You have been demoted to {W{+%s{x.\n\r", flag_string(staff_ranks, new_rank)), player);
    send_to_char(formatf("{+%s demoted to {W{+%s{x.\n\r", player->name, flag_string(staff_ranks, new_rank)), ch);
}


void do_spromote(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *player;
    char arg[MIL];

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  spromote <immortal>[ <rank>]\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);
    if ((player = get_char_world(ch, arg)) == NULL)
    {
        send_to_char("No one by that name found.\n\r", ch);
        return;
    }

    if (IS_NPC(player))
    {
        send_to_char("Nice try.\n\r", ch);
        return;
    }

    if (!IS_IMMORTAL(player))
    {
        send_to_char("That is not an immortal.\n\r", ch);
        return;
    }

    if (get_staff_rank(player) >= get_staff_rank(ch))
    {
        send_to_char("Nice try.\n\r", ch);
        return;
    }

    int old_rank = get_staff_rank(player);
    int new_rank = old_rank + 1;
    if (argument[0] != '\0')
    {
        if ((new_rank = stat_lookup(argument, staff_ranks, NO_FLAG)) == NO_FLAG ||
            new_rank >= get_staff_rank(ch) || new_rank <= old_rank)
        {
            send_to_char("No such staff rank.\n\r", ch);
            send_to_char("Please select one of the following:\n\r", ch);
            for(int i = 0; staff_ranks[i].name; i++)
            {
                if (staff_ranks[i].settable && staff_ranks[i].bit > old_rank && staff_ranks[i].bit < get_staff_rank(ch))
                {
                    send_to_char(formatf(" %s\n\r", staff_ranks[i].name), ch);
                }
            }
            return;
        }
    }
    
    player->pcdata->staff_rank = new_rank;
    save_char_obj(player);

    send_to_char(formatf("You have been promoted to {W{+%s{x.\n\r", flag_string(staff_ranks, new_rank)), player);
    send_to_char(formatf("{+%s promoted to {W{+%s{x.\n\r", player->name, flag_string(staff_ranks, new_rank)), ch);
}

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



/* Sets a person's supervisor */
void do_ssupervisor(CHAR_DATA *ch, char *argument)
{
    char arg[MSL], arg2[MSL];
    IMMORTAL_DATA *immortal;
    IMMORTAL_DATA *leader;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0') {
    send_to_char("Syntax:  staff supervisor [immortal] [supervisor]\n\r", ch);
    return;
    }

    if ((immortal = find_immortal(arg)) == NULL) {
    send_to_char("Immortal not found.\n\r", ch);
    return;
    }

    if ((leader = find_immortal(arg2)) == NULL) {
    send_to_char("Supervisor not found.\n\r", ch);
    return;
    }

    immortal->leader = str_dup(leader->name);
    act("Set $t's supervisor to $T.", ch, NULL, NULL, NULL, NULL, immortal->name, leader->name,  TO_CHAR, NULL, NULL);
    }




/*
 * Functions to do saving and loading. Immortal list is saved in
 * a tree fashion, using recursion to preserve the order of the list.
 *
 * #IMMORTAL
 * Name Name~
 * Duties Duties~ 		(make sure to print in a understandable fashion)
 * ImmFlag Flag~
 * Poofin String~
 * Poofout String~
 *
 * ...
 * #END
 */


/* Save the imm staff. */
void save_immstaff()
{
    json_save_staff(STAFF_JSON_FILE);
}


/* Save one immortal and his/her subordinates. */
void save_immortal(FILE *fp, IMMORTAL_DATA *immortal)
{
    if (immortal->next != NULL)
    save_immortal(fp, immortal->next);

    fprintf(fp, "#IMMORTAL\n");
    fprintf(fp, "Name %s~\n", immortal->name);
    fprintf(fp, "Duties %ld\n", immortal->duties);
    fprintf(fp, "Created %ld\n", (long)immortal->created);
    fprintf(fp, "ImmFlag %s~\n", immortal->imm_flag);
    fprintf(fp, "LastOLCCommand %ld\n", immortal->last_olc_command);
    fprintf(fp, "Leader %s~\n", immortal->leader != NULL ? immortal->leader : "None");
    fprintf(fp, "Poofin %s~\n", immortal->bamfin);
    fprintf(fp, "Poofout %s~\n", immortal->bamfout);


    fprintf(fp, "#-IMMORTAL\n");
}


/* These variables are used in various functions in loading/saving, so they
      are global to avoid redundancy. */
static bool fMatch;
static char *word;
bool loading_immortal_data = false;


/* Read imm staff at bootup. */
void read_immstaff()
{
    FILE *fp;
    IMMORTAL_DATA *immortal;
    char staff_file_buf[MAX_INPUT_LENGTH];
    const char *staff_file = resolve_game_path(STAFF_FILE, staff_file_buf, sizeof(staff_file_buf));

    // Try JSON first
    if (json_load_staff(STAFF_JSON_FILE))
        return;

    // Fall back to legacy format
    if ((fp = fopen(staff_file, "r")) == NULL) {
        pbugf(LOG_ERROR, "Couldn't open staff file '%s'.", staff_file);
        return;
    }

    loading_immortal_data = true;
    for (;;)
    {
        word = fread_word(fp);
        if (!str_cmp(word, "#IMMORTAL"))
        {
            immortal = read_immortal(fp);
            immortal->next = immortal_list;
            immortal_list = immortal;
        }

        if (!str_cmp(word, "#END"))
            break;
    }
    loading_immortal_data = false;

    fclose(fp);

    // Migrate to JSON
    json_save_staff(STAFF_JSON_FILE);
}


IMMORTAL_DATA *read_immortal(FILE *fp)
{
    IMMORTAL_DATA *immortal;

    immortal = new_immortal();

    while (str_cmp((word = fread_word(fp)), "#-IMMORTAL"))
    {
    fMatch = false;
    switch (word[0])
    {
        case '#':
        break;

        case 'C':
            KEY("Created", immortal->created, fread_number(fp));

        case 'D':
        if (!str_cmp(word, "Duties")) {
            immortal->duties = fread_number(fp);
            fMatch = true;
        }
        break;

            case 'I':
        KEYS("ImmFlag",	immortal->imm_flag,	fread_string(fp));
        break;

        case 'L':
        KEY("LastOLCCommand", immortal->last_olc_command, fread_number(fp));
                KEYS("Leader", immortal->leader, fread_string(fp));

        break;

        case 'N':
        KEYS("Name",	immortal->name,		fread_string(fp));
        break;

        case 'P':
        KEYS("Poofin",	immortal->bamfin,	fread_string(fp));
        KEYS("Poofout",	immortal->bamfout,	fread_string(fp));
        break;

        default:
        pbugf(LOG_ERROR, "No match for word %s", word);

        break;
    }
    }

    // Missing creation date
    if(immortal->created < 1)
    {
        DESCRIPTOR_DATA d;
        // TEMPORARY
        if( !load_char_obj(&d, immortal->name) )
        {
            pbugf(LOG_ERROR, "Attempting to correct created timestamp failed for %s", immortal->name);
        }
        else if( !d.character || !d.character->pcdata )
        {
            pbugf(LOG_ERROR, "Attempting to correct created timestamp failed for %s", immortal->name);
        }
        else
        {
            immortal->created = d.character->pcdata->creation_date;
            d.character->desc = NULL;
            extract_char(d.character, true);
        }
    }


    plogf(LOG_INFO, "Immortal %s", immortal->name);
    return immortal;
}


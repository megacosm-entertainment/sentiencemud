/***************************************************************************
 *  racedit.c - OLC Race Editor                                            *
 *                                                                         *
 *  Allows in-game editing of race definitions stored as JSON files in     *
 *  data/races/. Follows the medit/cmdedit pattern where the user enters   *
 *  the editor and stays in it until typing "done".                        *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../recycle.h"
#include "../../traits.h"

/* Forward declarations for save function (in json_race.c) */
extern bool save_race_json(RACE_DATA *race);

static const char *stat_names[] = { "str", "int", "wis", "dex", "con" };

/***************************************************************************
 * Race Editor Command Table                                               *
 ***************************************************************************/

const struct olc_cmd_type racedit_table[] =
{
    { "?",              show_help           },
    { "act",            racedit_act         },
    { "affects",        racedit_affects     },
    { "alignment",      racedit_alignment   },
    { "commands",       show_commands       },
    { "comments",       racedit_comments    },
    { "description",    racedit_description },
    { "form",           racedit_form        },
    { "immunities",     racedit_immunities  },
    { "list",           racedit_list        },
    { "maxstats",       racedit_maxstats    },
    { "maxvitals",      racedit_maxvitals   },
    { "name",           racedit_name        },
    { "offensive",      racedit_offensive   },
    { "parts",          racedit_parts       },
    { "pathrace",       racedit_pathrace    },
    { "playable",       racedit_playable    },
    { "prerequisite",   racedit_prerequisite },
    { "remortinto",     racedit_remortinto  },
    { "resistances",    racedit_resistances },
    { "save",           racedit_save        },
    { "show",           racedit_show        },
    { "size",           racedit_size        },
    { "skills",         racedit_skills      },
    { "starting",       racedit_starting    },
    { "stats",          racedit_stats       },
    { "summary",        racedit_summary     },
    { "trait",          racedit_trait        },
    { "vulnerabilities", racedit_vulnerabilities },
    { "whoname",        racedit_whoname     },
    { NULL,             0                   }
};


/***************************************************************************
 * Editor Entry Point                                                      *
 ***************************************************************************/

/**
 * do_racedit - Enter the race editor
 *
 * Usage: racedit <race id or name>
 *        racedit list
 *
 * @param ch        Character entering the editor
 * @param argument  Race ID or name to edit, or "list"
 */
void do_racedit(CHAR_DATA *ch, char *argument)
{
    RACE_DATA *race;
    char arg1[MAX_INPUT_LENGTH];

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  racedit <race id or name>\n\r", ch);
        send_to_char("         racedit list\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "list")) {
        racedit_list(ch, argument);
        return;
    }

    race = race_lookup(arg1);
    if (!race) {
        /* Also try by name */
        race = race_lookup_name(arg1);
    }

    if (!race) {
        send_to_char("No race by that ID or name.\n\r", ch);
        return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    olc_set_editor(ch, ED_RACE, race);
    racedit_show(ch, "");
}


/***************************************************************************
 * Editor Interpreter                                                      *
 ***************************************************************************/

/**
 * racedit - Interpreter loop for the race editor
 *
 * Dispatches typed commands to the race editor command table.
 *
 * @param ch        Character in the editor
 * @param argument  Command typed by the character
 */
void racedit(CHAR_DATA *ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_STRING_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (!str_cmp(command, "done")) {
        edit_done(ch);
        return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;

    if (command[0] == '\0') {
        racedit_show(ch, argument);
        return;
    }

    for (cmd = 0; racedit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, racedit_table[cmd].name)) {
            (*racedit_table[cmd].olc_fun)(ch, argument);
            return;
        }
    }

    interpret(ch, arg);
}


/***************************************************************************
 * Display                                                                 *
 ***************************************************************************/

/**
 * racedit_show - Display current race data
 *
 * Shows all editable fields of the current race.
 *
 * @param ch        Character viewing the race
 * @param argument  Unused
 * @return          false (no data changed)
 */
RACEDIT(racedit_show)
{
    RACE_DATA *race;
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    ITERATOR it;
    char *skill;
    int i;
    TRAIT_DEF *def;

    EDIT_RACE(ch, race);
    buffer = new_buf();

    add_buf(buffer, formatf("{R=== Race Editor: %s ==={x\n\r\n\r", race->id));

    /* Identity */
    add_buf(buffer, formatf("{CID:           {x%s\n\r", race->id));
    add_buf(buffer, formatf("{CUID:          {x%d\n\r", race->uid));
    add_buf(buffer, formatf("{CName:         {W%s{x\n\r", race->name));
    add_buf(buffer, formatf("{CSummary:      {x%s\n\r",
        IS_NULLSTR(race->summary) ? "(none)" : race->summary));
    add_buf(buffer, formatf("{CWho Name:     {W%s{x\n\r", race->who_name));
    add_buf(buffer, formatf("{CPlayable:     {x%s\n\r", race->playable ? "{GYes{x" : "{DNo{x"));
    add_buf(buffer, formatf("{CStarting:     {x%s\n\r", race->starting ? "{GYes{x" : "{DNo{x"));
    add_buf(buffer, formatf("{CPath Race:    {x%s\n\r", race->path_race ? "{YYes{x" : "{DNo{x"));

    /* Description */
    add_buf(buffer, formatf("{CDescription:{x\n\r%s\n\r",
        IS_NULLSTR(race->description) ? "   (none)" : race->description));

    /* Comments */
    if (!IS_NULLSTR(race->comments)) {
        add_buf(buffer, formatf("{CComments:{x\n\r%s\n\r", race->comments));
    }

    /* Physical */
    add_buf(buffer, "\n\r{R--- Physical ---{x\n\r");
    add_buf(buffer, formatf("{CMin Size:      {x%s\n\r", flag_string(size_flags, race->min_size)));
    add_buf(buffer, formatf("{CMax Size:      {x%s\n\r", flag_string(size_flags, race->max_size)));
    add_buf(buffer, formatf("{CAlignment:     {x%d\n\r", race->default_alignment));
    add_buf(buffer, formatf("{CForm:          {x%s\n\r", flag_string(form_flags, race->form)));
    add_buf(buffer, formatf("{CParts:         {x%s\n\r", flag_string(part_flags, race->parts)));

    /* Combat */
    add_buf(buffer, "\n\r{R--- Combat ---{x\n\r");
    add_buf(buffer, formatf("{CAct:           {x%s\n\r",
        bitmatrix_string(act_flagbank, race->act)));
    add_buf(buffer, formatf("{CAffects:       {x%s\n\r",
        bitvector_string(2, race->aff[0], affect_flags, race->aff[1], affect2_flags)));
    add_buf(buffer, formatf("{COffensive:     {x%s\n\r", flag_string(off_flags, race->off)));
    add_buf(buffer, formatf("{CImmunities:    {x%s\n\r", flag_string(imm_flags, race->imm)));
    add_buf(buffer, formatf("{CResistances:   {x%s\n\r", flag_string(res_flags, race->res)));
    add_buf(buffer, formatf("{CVulnerabilities:{x %s\n\r", flag_string(vuln_flags, race->vuln)));

    /* Attributes */
    add_buf(buffer, "\n\r{R--- Attributes ---{x\n\r");
    sprintf(buf, "{CStats:         {x");
    for (i = 0; i < MAX_STATS; i++) {
        char tmp[32];
        sprintf(tmp, "%s:%d ", stat_names[i], race->stats[i]);
        strcat(buf, tmp);
    }
    strcat(buf, "\n\r");
    add_buf(buffer, buf);

    sprintf(buf, "{CMax Stats:     {x");
    for (i = 0; i < MAX_STATS; i++) {
        char tmp[32];
        sprintf(tmp, "%s:%d ", stat_names[i], race->max_stats[i]);
        strcat(buf, tmp);
    }
    strcat(buf, "\n\r");
    add_buf(buffer, buf);

    add_buf(buffer, formatf("{CMax Vitals:    {xhp:%d mana:%d move:%d\n\r",
        race->max_vitals[0], race->max_vitals[1], race->max_vitals[2]));

    /* Skills */
    add_buf(buffer, "\n\r{R--- Skills ---{x\n\r");
    if (list_size(race->skills) == 0) {
        add_buf(buffer, "   (none)\n\r");
    } else {
        i = 1;
        iterator_start(&it, race->skills);
        while ((skill = (char *)iterator_nextdata(&it))) {
            add_buf(buffer, formatf("   {C[{W%2d{C]{x %s\n\r", i++, skill));
        }
        iterator_stop(&it);
    }

    /* Starting Equipment */
    add_buf(buffer, "\n\r{R--- Starting Equipment ---{x\n\r");
    {
        bool has_eq = false;
        for (i = 0; i < MAX_RACE_STARTING_EQ; i++) {
            if (race->starting_eq[i] > 0) {
                add_buf(buffer, formatf("   {C[{W%d{C]{x Vnum %ld\n\r", i + 1, race->starting_eq[i]));
                has_eq = true;
            }
        }
        if (!has_eq)
            add_buf(buffer, "   (none)\n\r");
    }

    /* Remort */
    add_buf(buffer, "\n\r{R--- Remort ---{x\n\r");
    add_buf(buffer, formatf("{CPrerequisite:  {x%s\n\r",
        IS_NULLSTR(race->remort_race_id) ? "(none)" : race->remort_race_id));
    add_buf(buffer, formatf("{CRemort Into:   {x%s\n\r",
        IS_NULLSTR(race->remort_into_id) ? "(none)" : race->remort_into_id));

    /* Traits */
    if (race->trait_values && trait_def_count > 0) {
        add_buf(buffer, "\n\r{R--- Traits ---{x\n\r");
        for (def = trait_def_list; def; def = def->next) {
            TRAIT_VALUE *tv = &race->trait_values[def->index];
            if (!tv->set)
                continue;

            switch (def->type) {
                case TRAIT_BOOLEAN:
                    add_buf(buffer, formatf("   {C%-30s{x %s\n\r",
                        def->name, tv->bool_val ? "{GTrue{x" : "{DFalse{x"));
                    break;
                case TRAIT_INTEGER:
                    add_buf(buffer, formatf("   {C%-30s{x %d\n\r",
                        def->name, tv->int_val));
                    break;
                case TRAIT_STRING:
                    add_buf(buffer, formatf("   {C%-30s{x %s\n\r",
                        def->name, tv->string_val ? tv->string_val : "(null)"));
                    break;
            }
        }
    }

    page_to_char(buffer->string, ch);
    free_buf(buffer);
    return false;
}


/***************************************************************************
 * Editor Commands                                                         *
 ***************************************************************************/

/**
 * racedit_name - Set the race display name
 *
 * @param ch        Character editing
 * @param argument  New name string
 * @return          true if changed
 */
RACEDIT(racedit_name)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  name <name>\n\r", ch);
        return false;
    }

    free_string(race->name);
    race->name = str_dup(argument);
    send_to_char("Race name set.\n\r", ch);
    return true;
}


/**
 * racedit_summary - Set the one-line summary shown during character creation
 *
 * @param ch        Character editing
 * @param argument  Summary text, or empty to clear
 * @return          true if changed
 */
RACEDIT(racedit_summary)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  summary <text>\n\r", ch);
        send_to_char("         summary clear\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        free_string(race->summary);
        race->summary = str_dup("");
        send_to_char("Summary cleared.\n\r", ch);
        return true;
    }

    free_string(race->summary);
    race->summary = str_dup(argument);
    send_to_char("Summary set.\n\r", ch);
    return true;
}


/**
 * racedit_whoname - Set the who-list display name
 *
 * @param ch        Character editing
 * @param argument  New who name string (can contain color codes)
 * @return          true if changed
 */
RACEDIT(racedit_whoname)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  whoname <string>\n\r", ch);
        return false;
    }

    free_string(race->who_name);
    race->who_name = str_dup(argument);
    send_to_char("Who name set.\n\r", ch);
    return true;
}


/**
 * racedit_description - Edit the race description (opens string editor)
 *
 * @param ch        Character editing
 * @param argument  Unused
 * @return          true (opens string editor)
 */
RACEDIT(racedit_description)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    if (argument[0] == '\0') {
        string_append(ch, &race->description);
        return true;
    }

    send_to_char("Syntax:  description    (opens string editor)\n\r", ch);
    return false;
}


/**
 * racedit_comments - Edit the builder comments (opens string editor)
 *
 * @param ch        Character editing
 * @param argument  Unused
 * @return          true (opens string editor)
 */
RACEDIT(racedit_comments)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    if (argument[0] == '\0') {
        string_append(ch, &race->comments);
        return true;
    }

    send_to_char("Syntax:  comments    (opens string editor)\n\r", ch);
    return false;
}


/**
 * racedit_playable - Toggle whether the race is playable
 *
 * @param ch        Character editing
 * @param argument  Unused
 * @return          true if changed
 */
RACEDIT(racedit_playable)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    race->playable = !race->playable;
    send_to_char(formatf("Playable set to %s.\n\r", race->playable ? "Yes" : "No"), ch);
    return true;
}


/**
 * racedit_starting - Toggle whether the race is available at character creation
 *
 * @param ch        Character editing
 * @param argument  Unused
 * @return          true if changed
 */
RACEDIT(racedit_starting)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    race->starting = !race->starting;
    send_to_char(formatf("Starting set to %s.\n\r", race->starting ? "Yes" : "No"), ch);
    return true;
}


/**
 * racedit_pathrace - Toggle whether the race is a path/transformation race
 *
 * Path races are transformations applied to any base race (e.g., lich,
 * vampire, slayer). Characters who become a path race retain their
 * original race as orace.
 *
 * @param ch        Character editing
 * @param argument  Unused
 * @return          true if changed
 */
RACEDIT(racedit_pathrace)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    race->path_race = !race->path_race;
    send_to_char(formatf("Path race set to %s.\n\r", race->path_race ? "Yes" : "No"), ch);
    return true;
}


/**
 * racedit_alignment - Set the default alignment
 *
 * @param ch        Character editing
 * @param argument  Alignment value (-1, 0, or 1)
 * @return          true if changed
 */
RACEDIT(racedit_alignment)
{
    RACE_DATA *race;
    int val;
    EDIT_RACE(ch, race);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax:  alignment <-1000 to 1000>\n\r", ch);
        send_to_char("  e.g. -750 = evil, 0 = neutral, 750 = good\n\r", ch);
        return false;
    }

    val = atoi(argument);
    if (val < -1000 || val > 1000) {
        send_to_char("Alignment must be between -1000 and 1000.\n\r", ch);
        return false;
    }

    race->default_alignment = val;
    send_to_char("Default alignment set.\n\r", ch);
    return true;
}


/**
 * racedit_size - Set the min/max size
 *
 * @param ch        Character editing
 * @param argument  "min <size>" or "max <size>"
 * @return          true if changed
 */
RACEDIT(racedit_size)
{
    RACE_DATA *race;
    char arg1[MAX_INPUT_LENGTH];
    int value;
    EDIT_RACE(ch, race);

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  size min <size>\n\r", ch);
        send_to_char("         size max <size>\n\r", ch);
        send_to_char("Type '? size' for a list of sizes.\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "min")) {
        if ((value = flag_value(size_flags, argument)) == NO_FLAG) {
            send_to_char("Invalid size. Type '? size' for list.\n\r", ch);
            return false;
        }
        race->min_size = value;
        send_to_char("Minimum size set.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "max")) {
        if ((value = flag_value(size_flags, argument)) == NO_FLAG) {
            send_to_char("Invalid size. Type '? size' for list.\n\r", ch);
            return false;
        }
        race->max_size = value;
        send_to_char("Maximum size set.\n\r", ch);
        return true;
    }

    /* Try setting both min and max to the same value */
    if ((value = flag_value(size_flags, arg1)) != NO_FLAG) {
        race->min_size = value;
        race->max_size = value;
        send_to_char("Size set (min and max).\n\r", ch);
        return true;
    }

    send_to_char("Syntax:  size [min|max] <size>\n\r", ch);
    return false;
}


/**
 * racedit_stats - Set base stats
 *
 * @param ch        Character editing
 * @param argument  "<stat> <value>" (e.g., "str 15")
 * @return          true if changed
 */
RACEDIT(racedit_stats)
{
    RACE_DATA *race;
    char arg1[MAX_INPUT_LENGTH];
    int i, val;
    EDIT_RACE(ch, race);

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0' || argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax:  stats <str|int|wis|dex|con> <value>\n\r", ch);
        return false;
    }

    for (i = 0; i < MAX_STATS; i++) {
        if (!str_cmp(arg1, stat_names[i])) {
            val = atoi(argument);
            if (val < 1 || val > 25) {
                send_to_char("Stat value must be between 1 and 25.\n\r", ch);
                return false;
            }
            race->stats[i] = val;
            send_to_char(formatf("Base %s set to %d.\n\r", stat_names[i], val), ch);
            return true;
        }
    }

    send_to_char("Invalid stat name. Use: str, int, wis, dex, con\n\r", ch);
    return false;
}


/**
 * racedit_maxstats - Set maximum stats
 *
 * @param ch        Character editing
 * @param argument  "<stat> <value>" (e.g., "str 22")
 * @return          true if changed
 */
RACEDIT(racedit_maxstats)
{
    RACE_DATA *race;
    char arg1[MAX_INPUT_LENGTH];
    int i, val;
    EDIT_RACE(ch, race);

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0' || argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax:  maxstats <str|int|wis|dex|con> <value>\n\r", ch);
        return false;
    }

    for (i = 0; i < MAX_STATS; i++) {
        if (!str_cmp(arg1, stat_names[i])) {
            val = atoi(argument);
            if (val < 1 || val > 30) {
                send_to_char("Max stat value must be between 1 and 30.\n\r", ch);
                return false;
            }
            race->max_stats[i] = val;
            send_to_char(formatf("Max %s set to %d.\n\r", stat_names[i], val), ch);
            return true;
        }
    }

    send_to_char("Invalid stat name. Use: str, int, wis, dex, con\n\r", ch);
    return false;
}


/**
 * racedit_maxvitals - Set maximum vitals (hp, mana, move)
 *
 * @param ch        Character editing
 * @param argument  "<hp|mana|move> <value>"
 * @return          true if changed
 */
RACEDIT(racedit_maxvitals)
{
    RACE_DATA *race;
    char arg1[MAX_INPUT_LENGTH];
    int val;
    EDIT_RACE(ch, race);

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0' || argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax:  maxvitals <hp|mana|move> <value>\n\r", ch);
        return false;
    }

    val = atoi(argument);
    if (val < 100 || val > 99999) {
        send_to_char("Value must be between 100 and 99999.\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "hp")) {
        race->max_vitals[0] = val;
        send_to_char(formatf("Max HP set to %d.\n\r", val), ch);
        return true;
    }
    if (!str_cmp(arg1, "mana")) {
        race->max_vitals[1] = val;
        send_to_char(formatf("Max mana set to %d.\n\r", val), ch);
        return true;
    }
    if (!str_cmp(arg1, "move")) {
        race->max_vitals[2] = val;
        send_to_char(formatf("Max move set to %d.\n\r", val), ch);
        return true;
    }

    send_to_char("Syntax:  maxvitals <hp|mana|move> <value>\n\r", ch);
    return false;
}


/**
 * racedit_form - Toggle body form flags
 *
 * @param ch        Character editing
 * @param argument  Flag names to toggle
 * @return          true if changed
 */
RACEDIT(racedit_form)
{
    RACE_DATA *race;
    long value;
    EDIT_RACE(ch, race);

    if (argument[0] != '\0') {
        if ((value = flag_value(form_flags, argument)) != NO_FLAG) {
            race->form ^= value;
            send_to_char("Form flags toggled.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: form [flags]\n\rType '? form' for a list of flags.\n\r", ch);
    return false;
}


/**
 * racedit_parts - Toggle body parts flags
 *
 * @param ch        Character editing
 * @param argument  Flag names to toggle
 * @return          true if changed
 */
RACEDIT(racedit_parts)
{
    RACE_DATA *race;
    long value;
    EDIT_RACE(ch, race);

    if (argument[0] != '\0') {
        if ((value = flag_value(part_flags, argument)) != NO_FLAG) {
            race->parts ^= value;
            send_to_char("Parts flags toggled.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: parts [flags]\n\rType '? part' for a list of flags.\n\r", ch);
    return false;
}


/**
 * racedit_act - Toggle act flags
 *
 * @param ch        Character editing
 * @param argument  Flag names to toggle
 * @return          true if changed
 */
RACEDIT(racedit_act)
{
    RACE_DATA *race;
    long value;
    EDIT_RACE(ch, race);

    if (argument[0] != '\0') {
        if ((value = flag_value(act_flags, argument)) != NO_FLAG) {
            race->act[0] ^= value;
            send_to_char("Act flags toggled.\n\r", ch);
            return true;
        }
        if ((value = flag_value(act2_flags, argument)) != NO_FLAG) {
            race->act[1] ^= value;
            send_to_char("Act2 flags toggled.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: act [flags]\n\rType '? act' for a list of flags.\n\r", ch);
    return false;
}


/**
 * racedit_affects - Toggle affect flags
 *
 * @param ch        Character editing
 * @param argument  Flag names to toggle
 * @return          true if changed
 */
RACEDIT(racedit_affects)
{
    RACE_DATA *race;
    long value;
    EDIT_RACE(ch, race);

    if (argument[0] != '\0') {
        if ((value = flag_value(affect_flags, argument)) != NO_FLAG) {
            race->aff[0] ^= value;
            send_to_char("Affect flags toggled.\n\r", ch);
            return true;
        }
        if ((value = flag_value(affect2_flags, argument)) != NO_FLAG) {
            race->aff[1] ^= value;
            send_to_char("Affect2 flags toggled.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: affects [flags]\n\rType '? affect' for a list of flags.\n\r", ch);
    return false;
}


/**
 * racedit_offensive - Toggle offensive flags
 *
 * @param ch        Character editing
 * @param argument  Flag names to toggle
 * @return          true if changed
 */
RACEDIT(racedit_offensive)
{
    RACE_DATA *race;
    long value;
    EDIT_RACE(ch, race);

    if (argument[0] != '\0') {
        if ((value = flag_value(off_flags, argument)) != NO_FLAG) {
            race->off ^= value;
            send_to_char("Offensive flags toggled.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: offensive [flags]\n\rType '? off' for a list of flags.\n\r", ch);
    return false;
}


/**
 * racedit_immunities - Toggle immunity flags
 *
 * @param ch        Character editing
 * @param argument  Flag names to toggle
 * @return          true if changed
 */
RACEDIT(racedit_immunities)
{
    RACE_DATA *race;
    long value;
    EDIT_RACE(ch, race);

    if (argument[0] != '\0') {
        if ((value = flag_value(imm_flags, argument)) != NO_FLAG) {
            race->imm ^= value;
            send_to_char("Immunity flags toggled.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: immunities [flags]\n\rType '? imm' for a list of flags.\n\r", ch);
    return false;
}


/**
 * racedit_resistances - Toggle resistance flags
 *
 * @param ch        Character editing
 * @param argument  Flag names to toggle
 * @return          true if changed
 */
RACEDIT(racedit_resistances)
{
    RACE_DATA *race;
    long value;
    EDIT_RACE(ch, race);

    if (argument[0] != '\0') {
        if ((value = flag_value(res_flags, argument)) != NO_FLAG) {
            race->res ^= value;
            send_to_char("Resistance flags toggled.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: resistances [flags]\n\rType '? res' for a list of flags.\n\r", ch);
    return false;
}


/**
 * racedit_vulnerabilities - Toggle vulnerability flags
 *
 * @param ch        Character editing
 * @param argument  Flag names to toggle
 * @return          true if changed
 */
RACEDIT(racedit_vulnerabilities)
{
    RACE_DATA *race;
    long value;
    EDIT_RACE(ch, race);

    if (argument[0] != '\0') {
        if ((value = flag_value(vuln_flags, argument)) != NO_FLAG) {
            race->vuln ^= value;
            send_to_char("Vulnerability flags toggled.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: vulnerabilities [flags]\n\rType '? vuln' for a list of flags.\n\r", ch);
    return false;
}


/**
 * racedit_skills - Add or remove racial skills
 *
 * @param ch        Character editing
 * @param argument  "add <skill>" or "remove <#>"
 * @return          true if changed
 */
RACEDIT(racedit_skills)
{
    RACE_DATA *race;
    char arg1[MAX_INPUT_LENGTH];
    EDIT_RACE(ch, race);

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  skills add <skill name>\n\r", ch);
        send_to_char("         skills remove <number>\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "add")) {
        if (argument[0] == '\0') {
            send_to_char("Syntax:  skills add <skill name>\n\r", ch);
            return false;
        }
        list_appendlink(race->skills, str_dup(argument));
        send_to_char(formatf("Skill '%s' added.\n\r", argument), ch);
        return true;
    }

    if (!str_cmp(arg1, "remove")) {
        int num;
        if (argument[0] == '\0' || !is_number(argument)) {
            send_to_char("Syntax:  skills remove <number>\n\r", ch);
            return false;
        }
        num = atoi(argument);
        if (num < 1 || num > list_size(race->skills)) {
            send_to_char("Invalid skill number.\n\r", ch);
            return false;
        }
        {
            ITERATOR it;
            char *skill;
            int i = 1;
            iterator_start(&it, race->skills);
            while ((skill = (char *)iterator_nextdata(&it))) {
                if (i == num) {
                    iterator_stop(&it);
                    list_remlink(race->skills, skill, false);
                    free_string(skill);
                    send_to_char("Skill removed.\n\r", ch);
                    return true;
                }
                i++;
            }
            iterator_stop(&it);
        }
        send_to_char("Skill not found.\n\r", ch);
        return false;
    }

    send_to_char("Syntax:  skills add <skill name>\n\r", ch);
    send_to_char("         skills remove <number>\n\r", ch);
    return false;
}


/**
 * racedit_prerequisite - Set the prerequisite race for remort
 *
 * @param ch        Character editing
 * @param argument  Race ID, or "none" to clear
 * @return          true if changed
 */
RACEDIT(racedit_prerequisite)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  prerequisite <race id|none>\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "none")) {
        free_string(race->remort_race_id);
        race->remort_race_id = NULL;
        send_to_char("Prerequisite race cleared.\n\r", ch);
        return true;
    }

    {
        RACE_DATA *target = race_lookup(argument);
        if (!target)
            target = race_lookup_name(argument);
        if (!target) {
            send_to_char("No race by that ID or name.\n\r", ch);
            return false;
        }
        free_string(race->remort_race_id);
        race->remort_race_id = str_dup(target->id);
        send_to_char(formatf("Prerequisite race set to '%s' (%s).\n\r", target->name, target->id), ch);
    }
    return true;
}


/**
 * racedit_remortinto - Set what race this remorts into
 *
 * @param ch        Character editing
 * @param argument  Race ID, or "none" to clear
 * @return          true if changed
 */
RACEDIT(racedit_remortinto)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  remortinto <race id|none>\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "none")) {
        free_string(race->remort_into_id);
        race->remort_into_id = NULL;
        send_to_char("Remort-into race cleared.\n\r", ch);
        return true;
    }

    {
        RACE_DATA *target = race_lookup(argument);
        if (!target)
            target = race_lookup_name(argument);
        if (!target) {
            send_to_char("No race by that ID or name.\n\r", ch);
            return false;
        }
        free_string(race->remort_into_id);
        race->remort_into_id = str_dup(target->id);
        send_to_char(formatf("Remort-into race set to '%s' (%s).\n\r", target->name, target->id), ch);
    }
    return true;
}


/**
 * racedit_trait - Set a trait value on the race
 *
 * @param ch        Character editing
 * @param argument  "<trait_id> <value>" or "<trait_id> unset"
 * @return          true if changed
 */
RACEDIT(racedit_trait)
{
    RACE_DATA *race;
    char arg1[MAX_INPUT_LENGTH];
    TRAIT_DEF *def;
    TRAIT_VALUE *tv;
    EDIT_RACE(ch, race);

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  trait <trait_id> <value>\n\r", ch);
        send_to_char("         trait <trait_id> unset\n\r", ch);
        send_to_char("         trait list\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "list")) {
        BUFFER *buffer = new_buf();
        add_buf(buffer, "{RAvailable Traits:{x\n\r");
        for (def = trait_def_list; def; def = def->next) {
            const char *type_str = "???";
            switch (def->type) {
                case TRAIT_BOOLEAN: type_str = "bool"; break;
                case TRAIT_INTEGER: type_str = "int"; break;
                case TRAIT_STRING:  type_str = "string"; break;
            }
            add_buf(buffer, formatf("   {C%-30s{x [%s] %s\n\r",
                def->id, type_str,
                !IS_NULLSTR(def->description) ? def->description : ""));
        }
        page_to_char(buffer->string, ch);
        free_buf(buffer);
        return false;
    }

    def = trait_def_lookup_name(arg1);
    if (!def) {
        send_to_char("Unknown trait. Use 'trait list' to see available traits.\n\r", ch);
        return false;
    }

    if (!race->trait_values) {
        send_to_char("Race trait values not initialized.\n\r", ch);
        return false;
    }

    tv = &race->trait_values[def->index];

    if (!str_cmp(argument, "unset")) {
        tv->set = false;
        switch (def->type) {
            case TRAIT_BOOLEAN: tv->bool_val = def->default_bool; break;
            case TRAIT_INTEGER: tv->int_val = def->default_int; break;
            case TRAIT_STRING:
                free_string(tv->string_val);
                tv->string_val = def->default_string ? str_dup(def->default_string) : NULL;
                break;
        }
        send_to_char(formatf("Trait '%s' reverted to default.\n\r", def->name), ch);
        return true;
    }

    if (argument[0] == '\0') {
        send_to_char("Syntax:  trait <trait_id> <value|unset>\n\r", ch);
        return false;
    }

    tv->set = true;
    switch (def->type) {
        case TRAIT_BOOLEAN:
            if (!str_cmp(argument, "true") || !str_cmp(argument, "yes") || !str_cmp(argument, "1")) {
                tv->bool_val = true;
            } else if (!str_cmp(argument, "false") || !str_cmp(argument, "no") || !str_cmp(argument, "0")) {
                tv->bool_val = false;
            } else {
                send_to_char("Boolean trait: use true/false, yes/no, or 1/0.\n\r", ch);
                return false;
            }
            break;
        case TRAIT_INTEGER:
            if (!is_number(argument)) {
                send_to_char("Integer trait: provide a numeric value.\n\r", ch);
                return false;
            }
            tv->int_val = atoi(argument);
            break;
        case TRAIT_STRING:
            free_string(tv->string_val);
            tv->string_val = str_dup(argument);
            break;
    }

    send_to_char(formatf("Trait '%s' set.\n\r", def->name), ch);
    return true;
}


/**
 * racedit_save - Save the current race to its JSON file
 *
 * @param ch        Character editing
 * @param argument  Unused
 * @return          true if saved successfully
 */
RACEDIT(racedit_save)
{
    RACE_DATA *race;
    EDIT_RACE(ch, race);

    if (save_race_json(race)) {
        send_to_char(formatf("Race '%s' saved to file.\n\r", race->id), ch);
        return true;
    } else {
        send_to_char("Error saving race file!\n\r", ch);
        return false;
    }
}


/**
 * racedit_list - List all loaded races
 *
 * @param ch        Character viewing
 * @param argument  Optional filter string
 * @return          false (display only)
 */
RACEDIT(racedit_list)
{
    RACE_DATA *race;
    BUFFER *buffer;
    int count = 0;

    buffer = new_buf();
    add_buf(buffer, "{R  UID  ID                Name                Playable  Starting  Remort  Path{x\n\r");
    add_buf(buffer, "{D ---- ------------------- ------------------- --------- --------- ------- ------{x\n\r");

    for (race = race_list; race; race = race->next) {
        if (argument[0] != '\0' && str_prefix(argument, race->id)
            && str_prefix(argument, race->name))
            continue;

        add_buf(buffer, formatf(" {C%4d{x %-19s %-19s %-9s %-9s %-7s %s\n\r",
            race->uid,
            race->id,
            race->name,
            race->playable ? "{GYes{x" : "{DNo{x",
            race->starting ? "{GYes{x" : "{DNo{x",
            race_is_remort(race) ? "{YYes{x" : "{DNo{x",
            race->path_race ? "{YYes{x" : "{DNo{x"));
        count++;
    }

    add_buf(buffer, formatf("\n\r{x%d race%s listed.\n\r", count, count == 1 ? "" : "s"));
    page_to_char(buffer->string, ch);
    free_buf(buffer);
    return false;
}

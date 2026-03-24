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
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"
#include "../../io/json/json_olc.h"

/* Forward declarations for save function (in json_race.c) */
extern bool save_race_json(RACE_DATA *race);

static const char *stat_names[] = { "str", "int", "wis", "dex", "con" };

/***************************************************************************
 * History Helpers                                                         *
 ***************************************************************************/

static OLC_CHANGE_HISTORY *racedit_get_history(void *pEdit)
{
    RACE_DATA *race = (RACE_DATA *)pEdit;
    return race ? (OLC_CHANGE_HISTORY *)race->olc_history : NULL;
}

static OLC_CHANGE_HISTORY *racedit_ensure_history(RACE_DATA *race)
{
    if (!race) return NULL;
    if (!race->olc_history)
        race->olc_history = olc_history_load(OLC_HIST_RACE, race->id);
    if (!race->olc_history)
        race->olc_history = olc_history_new();
    return (OLC_CHANGE_HISTORY *)race->olc_history;
}

/** Record a change and mark dirty in one step. */
static void racedit_record(RACE_DATA *race, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    olc_history_record(racedit_ensure_history(race), ch,
        field, old_val, new_val);
    olc_history_mark_dirty(OLC_HIST_RACE, race->id,
        (OLC_CHANGE_HISTORY *)race->olc_history);
}

/** Generic callback wrapper for olc_cmd_* helpers. */
static void racedit_record_cb(void *ctx, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    racedit_record((RACE_DATA *)ctx, ch, field, old_val, new_val);
}

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
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF racedit_def = {
    .name           = "RacEdit",
    .editor_type    = ED_RACE,
    .cmd_table      = racedit_table,
    .show_fn        = racedit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = true,
    .get_history_fn = racedit_get_history,
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

    if (!olc_editor_check_perm(ch, &racedit_def, NULL)) {
        send_to_char("You don't have permission to edit races.\n\r", ch);
        return;
    }

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

    olc_editor_enter(ch, &racedit_def, race, true);
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
    olc_editor_interp(ch, argument, &racedit_def);
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
    const OLC_EDITOR_THEME *theme = olc_get_theme(&racedit_def);
    OLC_LAYOUT_CTX *ctx;
    ITERATOR it;
    char *skill;
    int i;
    TRAIT_DEF *def;

    EDIT_RACE(ch, race);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "RacEdit", race->name,
        formatf("UID %d", race->uid), &racedit_def);

    /* Identity */
    olc_display_string(ctx, theme, "ID:", NULL, race->id);
    olc_display_string(ctx, theme, "Name:", "name", race->name);
    olc_display_string(ctx, theme, "Summary:", "summary", race->summary);
    olc_display_string(ctx, theme, "Who Name:", "who", race->who_name);
    olc_display_pair(ctx, theme,
        "Playable:", "playable", race->playable ? "Yes" : "No",
        "Starting:", "starting", race->starting ? "Yes" : "No");
    olc_display_bool(ctx, theme, "Path Race:", "pathrace", race->path_race);

    olc_display_text(ctx, theme, "Description:", "description", race->description);
    if (!IS_NULLSTR(race->comments))
        olc_display_text(ctx, theme, "Comments:", "comments", race->comments);

    /* Physical */
    olc_display_section(ctx, theme, "Physical");
    olc_display_pair(ctx, theme,
        "Min Size:", "minsize", flag_string(size_flags, race->min_size),
        "Max Size:", "maxsize", flag_string(size_flags, race->max_size));
    olc_display_number(ctx, theme, "Alignment:", "alignment", race->default_alignment);
    olc_display_flags(ctx, theme, "Form:", "form", form_flags, race->form);
    olc_display_flags(ctx, theme, "Parts:", "parts", part_flags, race->parts);

    /* Combat */
    olc_display_section(ctx, theme, "Combat");
    olc_display_string(ctx, theme, "Act:", "act",
        bitmatrix_string(act_flagbank, race->act));
    olc_display_string(ctx, theme, "Affects:", "affect",
        bitvector_string(2, race->aff[0], affect_flags, race->aff[1], affect2_flags));
    olc_display_flags(ctx, theme, "Offensive:", "off", off_flags, race->off);
    olc_display_flags(ctx, theme, "Immunities:", "imm", imm_flags, race->imm);
    olc_display_flags(ctx, theme, "Resistances:", "res", res_flags, race->res);
    olc_display_flags(ctx, theme, "Vulnerabilities:", "vuln", vuln_flags, race->vuln);

    /* Attributes */
    olc_display_section(ctx, theme, "Attributes");
    {
        char stats_buf[MSL];
        char max_stats_buf[MSL];
        stats_buf[0] = '\0';
        max_stats_buf[0] = '\0';
        for (i = 0; i < MAX_STATS; i++) {
            strcat(stats_buf, formatf("%s:%d ", stat_names[i], race->stats[i]));
            strcat(max_stats_buf, formatf("%s:%d ", stat_names[i], race->max_stats[i]));
        }
        olc_display_string(ctx, theme, "Stats:", "stats", stats_buf);
        olc_display_string(ctx, theme, "Max Stats:", "maxstats", max_stats_buf);
    }
    olc_display_string(ctx, theme, "Max Vitals:", "maxvitals",
        formatf("hp:%d mana:%d move:%d",
            race->max_vitals[0], race->max_vitals[1], race->max_vitals[2]));

    /* Skills */
    olc_display_section(ctx, theme, "Skills");
    if (list_size(race->skills) == 0) {
        olc_display_infof(ctx, theme, "   %s(none){x", theme->unset);
    } else {
        i = 1;
        iterator_start(&it, race->skills);
        while ((skill = (char *)iterator_nextdata(&it))) {
            olc_display_infof(ctx, theme, "   %s[%s%2d%s]{x %s%s{x",
                theme->border, theme->label, i++, theme->border,
                theme->value, skill);
        }
        iterator_stop(&it);
    }

    /* Starting Equipment */
    olc_display_section(ctx, theme, "Starting Equipment");
    {
        bool has_eq = false;
        for (i = 0; i < MAX_RACE_STARTING_EQ; i++) {
            if (race->starting_eq[i] > 0) {
                olc_display_infof(ctx, theme, "   %s[%s%d%s]{x Vnum %s%ld{x",
                    theme->border, theme->label, i + 1, theme->border,
                    theme->value, race->starting_eq[i]);
                has_eq = true;
            }
        }
        if (!has_eq)
            olc_display_infof(ctx, theme, "   %s(none){x", theme->unset);
    }

    /* Remort */
    olc_display_section(ctx, theme, "Remort");
    olc_display_string(ctx, theme, "Prerequisite:", "prereq", race->remort_race_id);
    olc_display_string(ctx, theme, "Remort Into:", "remortinto", race->remort_into_id);

    /* Traits */
    if (race->trait_values && trait_def_count > 0) {
        bool has_traits = false;
        for (def = trait_def_list; def; def = def->next) {
            if (race->trait_values[def->index].set) {
                has_traits = true;
                break;
            }
        }
        if (has_traits) {
            olc_display_section(ctx, theme, "Traits");
            for (def = trait_def_list; def; def = def->next) {
                TRAIT_VALUE *tv = &race->trait_values[def->index];
                if (!tv->set) continue;
                switch (def->type) {
                    case TRAIT_BOOLEAN:
                        olc_display_bool(ctx, theme,
                            formatf("  %s:", def->name), NULL, tv->bool_val);
                        break;
                    case TRAIT_INTEGER:
                        olc_display_number(ctx, theme,
                            formatf("  %s:", def->name), NULL, tv->int_val);
                        break;
                    case TRAIT_STRING:
                        olc_display_string(ctx, theme,
                            formatf("  %s:", def->name), NULL, tv->string_val);
                        break;
                }
            }
        }
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
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
    return olc_cmd_string(ch, argument, "Name", NULL, &race->name,
        OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, race, racedit_record_cb);
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
    return olc_cmd_string(ch, argument, "Summary", NULL, &race->summary,
        OLC_STR_CLEARABLE, race, racedit_record_cb);
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
    return olc_cmd_string(ch, argument, "Who Name", NULL, &race->who_name,
        OLC_STR_DEFAULT, race, racedit_record_cb);
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
    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &race->description, race, racedit_record_cb);
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
    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &race->comments, race, racedit_record_cb);
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
    return olc_cmd_bool(ch, argument, "Playable", NULL, &race->playable,
        race, racedit_record_cb);
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
    return olc_cmd_bool(ch, argument, "Starting", NULL, &race->starting,
        race, racedit_record_cb);
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
    return olc_cmd_bool(ch, argument, "Path Race", NULL, &race->path_race,
        race, racedit_record_cb);
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
    EDIT_RACE(ch, race);
    return olc_cmd_number(ch, argument, "Alignment",
        "Syntax: alignment <-1000 to 1000>\n\r"
        "  e.g. -750 = evil, 0 = neutral, 750 = good\n\r",
        &race->default_alignment, -1000, 1000, race, racedit_record_cb);
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
        racedit_record(race, ch, "min_size",
            flag_string(size_flags, race->min_size), flag_string(size_flags, value));
        race->min_size = value;
        send_to_char("Minimum size set.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "max")) {
        if ((value = flag_value(size_flags, argument)) == NO_FLAG) {
            send_to_char("Invalid size. Type '? size' for list.\n\r", ch);
            return false;
        }
        racedit_record(race, ch, "max_size",
            flag_string(size_flags, race->max_size), flag_string(size_flags, value));
        race->max_size = value;
        send_to_char("Maximum size set.\n\r", ch);
        return true;
    }

    /* Try setting both min and max to the same value */
    if ((value = flag_value(size_flags, arg1)) != NO_FLAG) {
        racedit_record(race, ch, "size",
            formatf("%s/%s", flag_string(size_flags, race->min_size),
                flag_string(size_flags, race->max_size)),
            flag_string(size_flags, value));
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
            racedit_record(race, ch, formatf("stat_%s", stat_names[i]),
                formatf("%d", race->stats[i]), formatf("%d", val));
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
            racedit_record(race, ch, formatf("maxstat_%s", stat_names[i]),
                formatf("%d", race->max_stats[i]), formatf("%d", val));
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
        racedit_record(race, ch, "max_hp",
            formatf("%d", race->max_vitals[0]), formatf("%d", val));
        race->max_vitals[0] = val;
        send_to_char(formatf("Max HP set to %d.\n\r", val), ch);
        return true;
    }
    if (!str_cmp(arg1, "mana")) {
        racedit_record(race, ch, "max_mana",
            formatf("%d", race->max_vitals[1]), formatf("%d", val));
        race->max_vitals[1] = val;
        send_to_char(formatf("Max mana set to %d.\n\r", val), ch);
        return true;
    }
    if (!str_cmp(arg1, "move")) {
        racedit_record(race, ch, "max_move",
            formatf("%d", race->max_vitals[2]), formatf("%d", val));
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
    EDIT_RACE(ch, race);
    return olc_cmd_flag_toggle(ch, argument, "Form", NULL, &race->form,
        form_flags, race, racedit_record_cb);
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
    EDIT_RACE(ch, race);
    return olc_cmd_flag_toggle(ch, argument, "Parts", NULL, &race->parts,
        part_flags, race, racedit_record_cb);
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
            racedit_record(race, ch, "act", argument,
                (race->act[0] & value) ? "removed" : "added");
            race->act[0] ^= value;
            send_to_char("Act flags toggled.\n\r", ch);
            return true;
        }
        if ((value = flag_value(act2_flags, argument)) != NO_FLAG) {
            racedit_record(race, ch, "act2", argument,
                (race->act[1] & value) ? "removed" : "added");
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
            racedit_record(race, ch, "affects", argument,
                (race->aff[0] & value) ? "removed" : "added");
            race->aff[0] ^= value;
            send_to_char("Affect flags toggled.\n\r", ch);
            return true;
        }
        if ((value = flag_value(affect2_flags, argument)) != NO_FLAG) {
            racedit_record(race, ch, "affects2", argument,
                (race->aff[1] & value) ? "removed" : "added");
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
    EDIT_RACE(ch, race);
    return olc_cmd_flag_toggle(ch, argument, "Offensive", NULL, &race->off,
        off_flags, race, racedit_record_cb);
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
    EDIT_RACE(ch, race);
    return olc_cmd_flag_toggle(ch, argument, "Immunities", NULL, &race->imm,
        imm_flags, race, racedit_record_cb);
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
    EDIT_RACE(ch, race);
    return olc_cmd_flag_toggle(ch, argument, "Resistances", NULL, &race->res,
        res_flags, race, racedit_record_cb);
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
    EDIT_RACE(ch, race);
    return olc_cmd_flag_toggle(ch, argument, "Vulnerabilities", NULL, &race->vuln,
        vuln_flags, race, racedit_record_cb);
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
        racedit_record(race, ch, "skills", "", argument);
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
                    racedit_record(race, ch, "skills", skill, "");
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
        racedit_record(race, ch, "prerequisite",
            race->remort_race_id ? race->remort_race_id : "(none)", "(none)");
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
        racedit_record(race, ch, "prerequisite",
            race->remort_race_id ? race->remort_race_id : "(none)", target->id);
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
        racedit_record(race, ch, "remortinto",
            race->remort_into_id ? race->remort_into_id : "(none)", "(none)");
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
        racedit_record(race, ch, "remortinto",
            race->remort_into_id ? race->remort_into_id : "(none)", target->id);
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
        bool append_ok = true;

        if (!add_buf(buffer, "{RAvailable Traits:{x\n\r")) {
            send_to_char("Trait list output exceeded buffer limits.\n\r", ch);
            free_buf(buffer);
            return false;
        }

        for (def = trait_def_list; def; def = def->next) {
            const char *type_str = "???";
            switch (def->type) {
                case TRAIT_BOOLEAN: type_str = "bool"; break;
                case TRAIT_INTEGER: type_str = "int"; break;
                case TRAIT_STRING:  type_str = "string"; break;
            }
            if (!add_buf(buffer, formatf("   {C%-30s{x [%s] %s\n\r",
                def->id, type_str,
                !IS_NULLSTR(def->description) ? def->description : ""))) {
                append_ok = false;
                break;
            }
        }

        if (!append_ok) {
            send_to_char("Trait list output exceeded buffer limits.\n\r", ch);
            free_buf(buffer);
            return false;
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
        racedit_record(race, ch, formatf("trait_%s", def->id), "(set)", "(unset)");
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

    racedit_record(race, ch, formatf("trait_%s", def->id), "(unset)", argument);
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
        olc_history_flush(OLC_HIST_RACE, race->id,
            (OLC_CHANGE_HISTORY *)race->olc_history);
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
    bool append_ok = true;
    int count = 0;

    buffer = new_buf();
    if (!add_buf(buffer, "{R  UID  ID                Name                Playable  Starting  Remort  Path{x\n\r")
        || !add_buf(buffer, "{D ---- ------------------- ------------------- --------- --------- ------- ------{x\n\r")) {
        send_to_char("Race list output exceeded buffer limits.\n\r", ch);
        free_buf(buffer);
        return false;
    }

    for (race = race_list; race; race = race->next) {
        if (argument[0] != '\0' && str_prefix(argument, race->id)
            && str_prefix(argument, race->name))
            continue;

        if (!add_buf(buffer, formatf(" {C%4d{x %-19s %-19s %-9s %-9s %-7s %s\n\r",
            race->uid,
            race->id,
            race->name,
            race->playable ? "{GYes{x" : "{DNo{x",
            race->starting ? "{GYes{x" : "{DNo{x",
            race_is_remort(race) ? "{YYes{x" : "{DNo{x",
            race->path_race ? "{YYes{x" : "{DNo{x"))) {
            append_ok = false;
            break;
        }
        count++;
    }

    if (append_ok && !add_buf(buffer, formatf("\n\r{x%d race%s listed.\n\r", count, count == 1 ? "" : "s")))
        append_ok = false;

    if (!append_ok) {
        send_to_char("Race list output exceeded buffer limits.\n\r", ch);
        free_buf(buffer);
        return false;
    }

    page_to_char(buffer->string, ch);
    free_buf(buffer);
    return false;
}

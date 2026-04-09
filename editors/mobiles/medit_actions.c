/**
 * @file medit_actions.c
 * @brief GMCP action handlers for the mobile editor (MEdit).
 *
 * Provides form_fn / stage_fn pairs for 3 list operations:
 *   addmprog, addquest, addreputation.
 *
 * Each form_fn builds a JSON array of field descriptors that the web
 * client renders as a form.  Each stage_fn validates submitted values
 * and stages them into the active changeset via olc_stage_list_add().
 */

#include <string.h>
#include <stdlib.h>
#include <jansson.h>

#include "../../merc.h"
#include "../../olc.h"
#include "../../tables.h"
#include "../../scripts.h"
#include "../../skill_data.h"
#include "../common/olc_changeset.h"
#include "../common/olc_commands.h"
#include "../common/olc_actions.h"
#include "../common/olc_display.h"

/* =========================================================================
 * 1. addmprog
 * ========================================================================= */

static json_t *medit_action_mprog_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "script", "label", "Script", "type", "widevnum",
        "required", true));

    /* Build trigger options from trigger_table (mprog-applicable entries) */
    json_t *trig_opts = json_array();
    for (int i = 0; i < trigger_table_size; i++) {
        if (trigger_table[i].mob)
            json_array_append_new(trig_opts, json_string(trigger_table[i].name));
    }
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:o}",
        "field", "trigger", "label", "Trigger", "type", "enum",
        "required", true, "options", trig_opts));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:s}",
        "field", "phrase", "label", "Phrase", "type", "string",
        "required", false, "default", "*"));

    return fields;
}

static bool medit_action_mprog_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *script_str  = json_string_value(json_object_get(values, "script"));
    const char *trigger_str = json_string_value(json_object_get(values, "trigger"));
    const char *phrase      = json_string_value(json_object_get(values, "phrase"));

    if (!script_str || !trigger_str) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    int tindex = trigger_index((char *)trigger_str, PRG_MPROG);
    if (tindex < 0) {
        snprintf(errbuf, errlen, "Unknown trigger: %s", trigger_str);
        return false;
    }

    /* Parse script widevnum */
    char wnum_buf[MAX_INPUT_LENGTH];
    strlcpy(wnum_buf, script_str, sizeof(wnum_buf));

    WNUM script_wnum;
    AREA_DATA *context = pMob->area;
    if (!parse_widevnum(wnum_buf, context, &script_wnum)) {
        snprintf(errbuf, errlen, "Invalid script widevnum: %s", script_str);
        return false;
    }

    json_t *val = json_pack("{s:I, s:I, s:i, s:s, s:s}",
        "script_auid",   (json_int_t)script_wnum.pArea->uid,
        "script_vnum",   (json_int_t)script_wnum.vnum,
        "trigger_index", tindex,
        "trigger_name",  trigger_name(tindex),
        "phrase",        phrase ? phrase : "*");

    olc_stage_list_add(cs, "mprogs", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 2. addquest
 * ========================================================================= */

static json_t *medit_action_quest_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "quest", "label", "Quest", "type", "widevnum",
        "required", true));

    return fields;
}

static bool medit_action_quest_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *quest_str = json_string_value(json_object_get(values, "quest"));

    if (!quest_str) {
        snprintf(errbuf, errlen, "Missing required field: quest");
        return false;
    }

    char wnum_buf[MAX_INPUT_LENGTH];
    strlcpy(wnum_buf, quest_str, sizeof(wnum_buf));

    WNUM wnum;
    AREA_DATA *context = pMob->area;
    if (!parse_widevnum(wnum_buf, context, &wnum) || !wnum.pArea || wnum.vnum < 1) {
        snprintf(errbuf, errlen, "Invalid quest widevnum: %s", quest_str);
        return false;
    }

    if (!get_quest_index_v2_wnum(wnum)) {
        snprintf(errbuf, errlen, "No v2 quest with that widevnum exists");
        return false;
    }

    json_t *val = json_pack("{s:I, s:I}",
        "auid", (json_int_t)wnum.pArea->uid,
        "vnum", (json_int_t)wnum.vnum);

    olc_stage_list_add(cs, "quests", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 3. addreputation
 * ========================================================================= */

static json_t *medit_action_reputation_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "reputation", "label", "Reputation", "type", "widevnum",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:s}",
        "field", "min_rank", "label", "Minimum Rank (0=none)", "type", "string",
        "required", false, "default", "none"));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:s}",
        "field", "max_rank", "label", "Maximum Rank (0=none)", "type", "string",
        "required", false, "default", "none"));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "points", "label", "Points", "type", "int",
        "required", true));

    return fields;
}

static bool medit_action_reputation_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *rep_str    = json_string_value(json_object_get(values, "reputation"));
    const char *min_str    = json_string_value(json_object_get(values, "min_rank"));
    const char *max_str    = json_string_value(json_object_get(values, "max_rank"));
    json_t *j_points       = json_object_get(values, "points");

    if (!rep_str || !j_points) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    char wnum_buf[MAX_INPUT_LENGTH];
    strlcpy(wnum_buf, rep_str, sizeof(wnum_buf));

    WNUM wnum;
    AREA_DATA *context = pMob->area;
    if (!parse_widevnum(wnum_buf, context, &wnum) || !wnum.pArea || wnum.vnum < 1) {
        snprintf(errbuf, errlen, "Invalid reputation widevnum: %s", rep_str);
        return false;
    }

    REPUTATION_INDEX_DATA *repIndex = get_reputation_index(wnum.pArea, wnum.vnum);
    if (!IS_VALID(repIndex)) {
        snprintf(errbuf, errlen, "No reputation with that widevnum");
        return false;
    }

    int min_rank = 0;
    if (min_str && str_cmp(min_str, "none") && min_str[0] != '\0') {
        min_rank = atoi(min_str);
        if (min_rank < 1 || min_rank > list_size(repIndex->ranks)) {
            snprintf(errbuf, errlen, "Invalid minimum rank: %s", min_str);
            return false;
        }
    }

    int max_rank = 0;
    if (max_str && str_cmp(max_str, "none") && max_str[0] != '\0') {
        max_rank = atoi(max_str);
        if (max_rank < 1 || max_rank > list_size(repIndex->ranks)) {
            snprintf(errbuf, errlen, "Invalid maximum rank: %s", max_str);
            return false;
        }
    }

    if (min_rank && max_rank && min_rank > max_rank) {
        snprintf(errbuf, errlen, "Minimum rank cannot be greater than maximum rank");
        return false;
    }

    long points = (long)json_integer_value(j_points);
    if (points == 0) {
        snprintf(errbuf, errlen, "Points must be non-zero");
        return false;
    }

    json_t *val = json_pack("{s:I, s:I, s:i, s:i, s:I}",
        "auid",      (json_int_t)wnum.pArea->uid,
        "vnum",      (json_int_t)wnum.vnum,
        "min_rank",  min_rank,
        "max_rank",  max_rank,
        "points",    (json_int_t)points);

    olc_stage_list_add(cs, "reputations", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * Handler Table and Registration
 * ========================================================================= */

static const olc_action_handler_t medit_actions[] = {
    { "addmprog",      ED_MOBILE, "inline", "Add MProg",      "mprogs",      medit_action_mprog_form,      medit_action_mprog_stage },
    { "addquest",      ED_MOBILE, "inline", "Add Quest",      "quests",      medit_action_quest_form,      medit_action_quest_stage },
    { "addreputation", ED_MOBILE, "inline", "Add Reputation", "reputations", medit_action_reputation_form, medit_action_reputation_stage },
    { NULL }
};

void medit_register_actions(void)
{
    olc_register_actions(medit_actions);
}

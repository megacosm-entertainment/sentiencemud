/**
 * @file redit_actions.c
 * @brief GMCP action handlers for the room editor (REdit).
 *
 * Provides form_fn / stage_fn pairs for 4 list operations:
 *   mreset, oreset, addrprog, addcdesc.
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
#include "../common/olc_changeset.h"
#include "../common/olc_commands.h"
#include "../common/olc_actions.h"
#include "../common/olc_display.h"

/* =========================================================================
 * 1. mreset  (mobile reset list)
 * ========================================================================= */

static json_t *redit_action_mreset_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "mobile", "label", "Mobile", "type", "widevnum",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i}",
        "field", "limit", "label", "Max In Game", "type", "int",
        "required", false, "default", MAX_MOB));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i}",
        "field", "count", "label", "Max In Room", "type", "int",
        "required", false, "default", 1));

    return fields;
}

static bool redit_action_mreset_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)entity;
    const char *mob_str = json_string_value(json_object_get(values, "mobile"));
    json_t *j_limit     = json_object_get(values, "limit");
    json_t *j_count     = json_object_get(values, "count");

    if (!mob_str) {
        snprintf(errbuf, errlen, "Missing required field: mobile");
        return false;
    }

    char wnum_buf[MAX_INPUT_LENGTH];
    strlcpy(wnum_buf, mob_str, sizeof(wnum_buf));

    WNUM mob_wnum;
    AREA_DATA *context = pRoom->area;
    if (!parse_widevnum(wnum_buf, context, &mob_wnum)) {
        snprintf(errbuf, errlen, "Invalid mobile widevnum: %s", mob_str);
        return false;
    }

    if (!get_mob_index(mob_wnum.pArea, mob_wnum.vnum)) {
        snprintf(errbuf, errlen, "No mobile has that vnum");
        return false;
    }

    int limit = j_limit ? (int)json_integer_value(j_limit) : MAX_MOB;
    int count = j_count ? (int)json_integer_value(j_count) : 1;

    json_t *val = json_pack("{s:I, s:I, s:i, s:i}",
        "mobile_auid", (json_int_t)mob_wnum.pArea->uid,
        "mobile_vnum", (json_int_t)mob_wnum.vnum,
        "limit",       limit,
        "count",       count);

    olc_stage_list_add(cs, "mresets", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 2. oreset  (object reset list)
 * ========================================================================= */

static json_t *redit_action_oreset_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "object", "label", "Object", "type", "widevnum",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i}",
        "field", "limit", "label", "Object Limit", "type", "int",
        "required", false, "default", 0));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:b}",
        "field", "hidden", "label", "Hidden", "type", "bool",
        "required", false, "default", false));

    return fields;
}

static bool redit_action_oreset_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)entity;
    const char *obj_str = json_string_value(json_object_get(values, "object"));
    json_t *j_limit     = json_object_get(values, "limit");
    json_t *j_hidden    = json_object_get(values, "hidden");

    if (!obj_str) {
        snprintf(errbuf, errlen, "Missing required field: object");
        return false;
    }

    char wnum_buf[MAX_INPUT_LENGTH];
    strlcpy(wnum_buf, obj_str, sizeof(wnum_buf));

    WNUM obj_wnum;
    AREA_DATA *context = pRoom->area;
    if (!parse_widevnum(wnum_buf, context, &obj_wnum)) {
        snprintf(errbuf, errlen, "Invalid object widevnum: %s", obj_str);
        return false;
    }

    if (!get_obj_index(obj_wnum.pArea, obj_wnum.vnum)) {
        snprintf(errbuf, errlen, "No object has that vnum");
        return false;
    }

    int limit  = j_limit  ? (int)json_integer_value(j_limit) : 0;
    bool hidden = j_hidden ? json_is_true(j_hidden) : false;

    json_t *val = json_pack("{s:I, s:I, s:i, s:b}",
        "object_auid", (json_int_t)obj_wnum.pArea->uid,
        "object_vnum", (json_int_t)obj_wnum.vnum,
        "limit",       limit,
        "hidden",      hidden);

    olc_stage_list_add(cs, "oresets", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 3. addrprog  (room script/program list)
 * ========================================================================= */

static json_t *redit_action_rprog_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "script", "label", "Script", "type", "widevnum",
        "required", true));

    /* Build trigger options from trigger_table (rprog-applicable entries) */
    json_t *trig_opts = json_array();
    for (int i = 0; i < trigger_table_size; i++) {
        if (trigger_table[i].room)
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

static bool redit_action_rprog_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)entity;
    const char *script_str  = json_string_value(json_object_get(values, "script"));
    const char *trigger_str = json_string_value(json_object_get(values, "trigger"));
    const char *phrase      = json_string_value(json_object_get(values, "phrase"));

    if (!script_str || !trigger_str) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    int tindex = trigger_index((char *)trigger_str, PRG_RPROG);
    if (tindex < 0) {
        snprintf(errbuf, errlen, "Unknown trigger: %s", trigger_str);
        return false;
    }

    char wnum_buf[MAX_INPUT_LENGTH];
    strlcpy(wnum_buf, script_str, sizeof(wnum_buf));

    WNUM script_wnum;
    AREA_DATA *context = pRoom->area;
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

    olc_stage_list_add(cs, "rprogs", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 4. addcdesc  (conditional description list)
 * ========================================================================= */

static json_t *redit_action_cdesc_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    /* Build condition options from room_condition_flags */
    json_t *cond_opts = olc_flag_options_json(room_condition_flags);
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:o}",
        "field", "condition", "label", "Condition", "type", "enum",
        "required", true, "options", cond_opts));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "phrase", "label", "Phrase", "type", "string",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:s}",
        "field", "description", "label", "Description", "type", "multiline",
        "required", false, "default", ""));

    return fields;
}

static bool redit_action_cdesc_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    (void)entity;
    const char *cond_str = json_string_value(json_object_get(values, "condition"));
    const char *phrase   = json_string_value(json_object_get(values, "phrase"));
    const char *desc     = json_string_value(json_object_get(values, "description"));

    if (!cond_str || !phrase) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    long cond_val = flag_value(room_condition_flags, cond_str);
    if (cond_val == NO_FLAG) {
        snprintf(errbuf, errlen, "Unknown condition: %s", cond_str);
        return false;
    }

    int phrase_val = cd_phrase_lookup((int)cond_val, (char *)phrase);
    if (phrase_val == -1) {
        snprintf(errbuf, errlen, "Invalid phrase for condition: %s", phrase);
        return false;
    }

    json_t *val = json_pack("{s:i, s:i, s:s}",
        "condition",   (int)cond_val,
        "phrase",      phrase_val,
        "description", desc ? desc : "");

    olc_stage_list_add(cs, "cdescs", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * Handler Table and Registration
 * ========================================================================= */

static const olc_action_handler_t redit_actions[] = {
    { "mreset",    ED_ROOM, "inline", "Mob Reset",     "mresets", redit_action_mreset_form, redit_action_mreset_stage },
    { "oreset",    ED_ROOM, "inline", "Obj Reset",     "oresets", redit_action_oreset_form, redit_action_oreset_stage },
    { "addrprog",  ED_ROOM, "inline", "Add RProg",     "rprogs",  redit_action_rprog_form,  redit_action_rprog_stage },
    { "addcdesc",  ED_ROOM, "inline", "Add Cond Desc", "cdescs",  redit_action_cdesc_form,  redit_action_cdesc_stage },
    { NULL }
};

void redit_register_actions(void)
{
    olc_register_actions(redit_actions);
}

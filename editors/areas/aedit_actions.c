/**
 * @file aedit_actions.c
 * @brief GMCP action handlers for the area editor (AEdit).
 *
 * Provides form_fn / stage_fn pairs for 2 list operations:
 *   addaprog, addtrade.
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
 * 1. addaprog  (area script/program list)
 * ========================================================================= */

static json_t *aedit_action_aprog_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "script", "label", "Script", "type", "widevnum",
        "required", true));

    /* Build trigger options from trigger_table (aprog-applicable entries) */
    json_t *trig_opts = json_array();
    for (int i = 0; i < trigger_table_size; i++) {
        if (trigger_table[i].area)
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

static bool aedit_action_aprog_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    AREA_DATA *pArea = (AREA_DATA *)entity;
    const char *script_str  = json_string_value(json_object_get(values, "script"));
    const char *trigger_str = json_string_value(json_object_get(values, "trigger"));
    const char *phrase      = json_string_value(json_object_get(values, "phrase"));

    if (!script_str || !trigger_str) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    int tindex = trigger_index((char *)trigger_str, PRG_APROG);
    if (tindex < 0) {
        snprintf(errbuf, errlen, "Unknown trigger: %s", trigger_str);
        return false;
    }

    char wnum_buf[MAX_INPUT_LENGTH];
    strlcpy(wnum_buf, script_str, sizeof(wnum_buf));

    WNUM script_wnum;
    AREA_DATA *context = pArea;
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

    olc_stage_list_add(cs, "aprogs", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 2. addtrade  (area trade route list)
 * ========================================================================= */

static json_t *aedit_action_trade_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "obj_vnum", "label", "Object Vnum", "type", "widevnum",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i}",
        "field", "replenish_time", "label", "Replenish Time", "type", "int",
        "required", true, "default", 0));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i}",
        "field", "replenish_amount", "label", "Replenish Amount", "type", "int",
        "required", true, "default", 0));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i}",
        "field", "max_qty", "label", "Max Quantity", "type", "int",
        "required", true, "default", 0));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i}",
        "field", "min_price", "label", "Min Price", "type", "int",
        "required", true, "default", 0));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i}",
        "field", "max_price", "label", "Max Price", "type", "int",
        "required", true, "default", 0));

    return fields;
}

static bool aedit_action_trade_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    AREA_DATA *pArea = (AREA_DATA *)entity;
    const char *obj_str = json_string_value(json_object_get(values, "obj_vnum"));
    json_t *j_rep_time  = json_object_get(values, "replenish_time");
    json_t *j_rep_amt   = json_object_get(values, "replenish_amount");
    json_t *j_max_qty   = json_object_get(values, "max_qty");
    json_t *j_min_price = json_object_get(values, "min_price");
    json_t *j_max_price = json_object_get(values, "max_price");

    if (!obj_str) {
        snprintf(errbuf, errlen, "Missing required field: obj_vnum");
        return false;
    }

    char wnum_buf[MAX_INPUT_LENGTH];
    strlcpy(wnum_buf, obj_str, sizeof(wnum_buf));

    WNUM obj_wnum;
    AREA_DATA *context = pArea;
    if (!parse_widevnum(wnum_buf, context, &obj_wnum)) {
        snprintf(errbuf, errlen, "Invalid object widevnum: %s", obj_str);
        return false;
    }

    OBJ_INDEX_DATA *pObj = get_obj_index(obj_wnum.pArea, obj_wnum.vnum);
    if (!pObj) {
        snprintf(errbuf, errlen, "No object has that vnum");
        return false;
    }

    if (pObj->value[0] == TRADE_NONE) {
        snprintf(errbuf, errlen, "That is not a valid trade item");
        return false;
    }

    long rep_time  = j_rep_time  ? (long)json_integer_value(j_rep_time)  : 0;
    long rep_amt   = j_rep_amt   ? (long)json_integer_value(j_rep_amt)   : 0;
    long max_qty   = j_max_qty   ? (long)json_integer_value(j_max_qty)   : 0;
    long min_price = j_min_price ? (long)json_integer_value(j_min_price) : 0;
    long max_price = j_max_price ? (long)json_integer_value(j_max_price) : 0;

    json_t *val = json_pack("{s:I, s:I, s:i, s:I, s:I, s:I, s:I, s:I}",
        "obj_auid",         (json_int_t)obj_wnum.pArea->uid,
        "obj_vnum",         (json_int_t)obj_wnum.vnum,
        "trade_type",       (int)pObj->value[0],
        "replenish_time",   (json_int_t)rep_time,
        "replenish_amount", (json_int_t)rep_amt,
        "max_qty",          (json_int_t)max_qty,
        "min_price",        (json_int_t)min_price,
        "max_price",        (json_int_t)max_price);

    olc_stage_list_add(cs, "trades", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * Handler Table and Registration
 * ========================================================================= */

static const olc_action_handler_t aedit_actions[] = {
    { "addaprog",  ED_AREA, "inline", "Add AProg",  "aprogs",  aedit_action_aprog_form,  aedit_action_aprog_stage },
    { "addtrade",  ED_AREA, "inline", "Add Trade",  "trades",  aedit_action_trade_form,  aedit_action_trade_stage },
    { NULL }
};

void aedit_register_actions(void)
{
    olc_register_actions(aedit_actions);
}

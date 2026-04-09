/**
 * @file oedit_actions.c
 * @brief GMCP action handlers for the object editor (OEdit).
 *
 * Provides form_fn / stage_fn pairs for 10 list operations:
 *   addaffect, addimmune, addspell, addskill, addcatalyst,
 *   addoprog, addquest, addwaypoint, addtype, removetype.
 *
 * Each form_fn builds a JSON array of field descriptors that the web
 * client renders as a form.  Each stage_fn validates submitted values
 * and stages them into the active changeset via olc_stage_list_add()
 * or olc_changeset_add_change().
 */

#include <string.h>
#include <stdlib.h>
#include <jansson.h>

#include "../../merc.h"
#include "../../olc.h"
#include "../../tables.h"
#include "../../scripts.h"
#include "../../item_types.h"
#include "../../skill_data.h"
#include "../common/olc_changeset.h"
#include "../common/olc_commands.h"
#include "../common/olc_actions.h"
#include "../common/olc_display.h"

/* =========================================================================
 * 1. addaffect
 * ========================================================================= */

static json_t *oedit_action_affect_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_t *loc_opts = olc_flag_options_json(apply_flags);
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:o}",
        "field", "location", "label", "Location", "type", "enum",
        "required", true, "options", loc_opts));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "modifier", "label", "Modifier", "type", "int",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i, s:i}",
        "field", "random", "label", "Random (0-100)", "type", "int",
        "required", true, "min", 0, "max", 100));

    return fields;
}

static bool oedit_action_affect_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *loc_str = json_string_value(json_object_get(values, "location"));
    json_t *j_mod    = json_object_get(values, "modifier");
    json_t *j_random = json_object_get(values, "random");

    if (!loc_str || !j_mod || !j_random) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    long loc_val = flag_value(apply_flags, loc_str);
    if (loc_val == NO_FLAG) {
        snprintf(errbuf, errlen, "Unknown location: %s", loc_str);
        return false;
    }

    int modifier = (int)json_integer_value(j_mod);
    int random   = (int)json_integer_value(j_random);

    json_t *val = json_pack("{s:s, s:s, s:i, s:i, s:i, s:i, s:i, s:i}",
        "where",     flag_string(apply_types, TO_OBJECT),
        "location",  flag_string(apply_flags, loc_val),
        "modifier",  modifier,
        "type",      -1,
        "duration",  -1,
        "bitvector", 0,
        "level",     (int)pObj->level,
        "random",    random);

    olc_stage_list_add(cs, "affects", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 2. addimmune
 * ========================================================================= */

static json_t *oedit_action_immune_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    /* Build a small options array for immune/resist/vuln */
    json_t *where_opts = json_array();
    json_array_append_new(where_opts, json_string(flag_string(apply_types, TO_IMMUNE)));
    json_array_append_new(where_opts, json_string(flag_string(apply_types, TO_RESIST)));
    json_array_append_new(where_opts, json_string(flag_string(apply_types, TO_VULN)));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:o}",
        "field", "where", "label", "Where", "type", "enum",
        "required", true, "options", where_opts));

    json_t *imm_opts = olc_flag_options_json(imm_flags);
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:o}",
        "field", "flags", "label", "Flags", "type", "flags",
        "required", true, "options", imm_opts));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i, s:i}",
        "field", "random", "label", "Random (0-100)", "type", "int",
        "required", true, "min", 0, "max", 100));

    return fields;
}

static bool oedit_action_immune_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *where_str = json_string_value(json_object_get(values, "where"));
    const char *flags_str = json_string_value(json_object_get(values, "flags"));
    json_t *j_random      = json_object_get(values, "random");

    if (!where_str || !flags_str || !j_random) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    int where = flag_value(apply_types, where_str);
    if (where != TO_IMMUNE && where != TO_RESIST && where != TO_VULN) {
        snprintf(errbuf, errlen, "Invalid where: must be immune, resist, or vuln");
        return false;
    }

    long bv = flag_value(imm_flags, flags_str);
    if (bv == NO_FLAG || bv == 0) {
        snprintf(errbuf, errlen, "Invalid immunity flag: %s", flags_str);
        return false;
    }

    int random = (int)json_integer_value(j_random);

    json_t *val = json_pack("{s:s, s:s, s:i, s:i, s:i, s:i, s:i, s:i}",
        "where",     flag_string(apply_types, where),
        "location",  flag_string(apply_flags, APPLY_NONE),
        "modifier",  0,
        "type",      -1,
        "duration",  -1,
        "bitvector", (int)bv,
        "level",     (int)pObj->level,
        "random",    random);

    olc_stage_list_add(cs, "affects", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 3. addspell
 * ========================================================================= */

static json_t *oedit_action_spell_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "spell", "label", "Spell Name", "type", "string",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "level", "label", "Spell Level", "type", "int",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i, s:i}",
        "field", "random", "label", "Random (0-100)", "type", "int",
        "required", true, "min", 0, "max", 100));

    return fields;
}

static bool oedit_action_spell_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    (void)entity;
    const char *spell_name = json_string_value(json_object_get(values, "spell"));
    json_t *j_level  = json_object_get(values, "level");
    json_t *j_random = json_object_get(values, "random");

    if (!spell_name || !j_level || !j_random) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    int sn = skill_lookup(spell_name);
    if (sn < 0) {
        snprintf(errbuf, errlen, "Unknown spell: %s", spell_name);
        return false;
    }

    SKILL_DATA *sk = skill_find_uid(sn);
    int level  = (int)json_integer_value(j_level);
    int random = (int)json_integer_value(j_random);

    json_t *val = json_pack("{s:i, s:s, s:i, s:i}",
        "spell_uid",  sn,
        "spell_name", sk ? sk->name : "unknown",
        "level",      level,
        "repop",      random);

    olc_stage_list_add(cs, "spells", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 4. addskill
 * ========================================================================= */

static json_t *oedit_action_skill_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "skill", "label", "Skill Name", "type", "string",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "level", "label", "Modifier", "type", "int",
        "required", true));

    return fields;
}

static bool oedit_action_skill_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *skill_name = json_string_value(json_object_get(values, "skill"));
    json_t *j_level = json_object_get(values, "level");

    if (!skill_name || !j_level) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    int sn = skill_lookup(skill_name);
    if (sn < 0) {
        snprintf(errbuf, errlen, "Unknown skill: %s", skill_name);
        return false;
    }

    int modifier = (int)json_integer_value(j_level);

    /* Match the staging path from oedit_addskill: TO_OBJECT, APPLY_SKILL+sn */
    json_t *val = json_pack("{s:s, s:i, s:i, s:i, s:i, s:i, s:i, s:i}",
        "where",     flag_string(apply_types, TO_OBJECT),
        "location",  APPLY_SKILL + sn,
        "modifier",  modifier,
        "type",      -1,
        "duration",  -1,
        "bitvector", 0,
        "level",     (int)pObj->level,
        "random",    100);

    olc_stage_list_add(cs, "affects", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 5. addcatalyst
 * ========================================================================= */

static json_t *oedit_action_catalyst_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_t *type_opts = olc_flag_options_json(catalyst_types);
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:o}",
        "field", "type", "label", "Type", "type", "enum",
        "required", true, "options", type_opts));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "charges", "label", "Charges", "type", "int",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "modifier", "label", "Strength", "type", "int",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:i, s:i}",
        "field", "random", "label", "Chance (0-100)", "type", "int",
        "required", true, "min", 0, "max", 100));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:s}",
        "field", "name", "label", "Custom Name", "type", "string",
        "required", false, "default", ""));

    return fields;
}

static bool oedit_action_catalyst_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    (void)entity;
    const char *type_str = json_string_value(json_object_get(values, "type"));
    json_t *j_charges  = json_object_get(values, "charges");
    json_t *j_modifier = json_object_get(values, "modifier");
    json_t *j_random   = json_object_get(values, "random");
    const char *name   = json_string_value(json_object_get(values, "name"));

    if (!type_str || !j_charges || !j_modifier || !j_random) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    int t = flag_value(catalyst_types, type_str);
    if (t == NO_FLAG) {
        snprintf(errbuf, errlen, "Unknown catalyst type: %s", type_str);
        return false;
    }

    int strength = (int)json_integer_value(j_modifier);
    int charges  = (int)json_integer_value(j_charges);
    int chance   = URANGE(1, (int)json_integer_value(j_random), 100);

    json_t *val = json_pack("{s:i, s:s, s:i, s:i, s:i, s:i, s:s}",
        "type_id",     t,
        "type_name",   flag_string(catalyst_types, t),
        "strength",    strength,
        "charges",     charges,
        "chance",      chance,
        "where",       TO_CATALYST_DORMANT,
        "custom_name", name ? name : "");

    olc_stage_list_add(cs, "catalysts", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 6. addoprog
 * ========================================================================= */

static json_t *oedit_action_oprog_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "script", "label", "Script", "type", "widevnum",
        "required", true));

    /* Build trigger options from trigger_table (oprog-applicable entries) */
    json_t *trig_opts = json_array();
    for (int i = 0; i < trigger_table_size; i++) {
        if (trigger_table[i].obj)
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

static bool oedit_action_oprog_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *script_str  = json_string_value(json_object_get(values, "script"));
    const char *trigger_str = json_string_value(json_object_get(values, "trigger"));
    const char *phrase      = json_string_value(json_object_get(values, "phrase"));

    if (!script_str || !trigger_str) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    int tindex = trigger_index((char *)trigger_str, PRG_OPROG);
    if (tindex < 0) {
        snprintf(errbuf, errlen, "Unknown trigger: %s", trigger_str);
        return false;
    }

    /* Parse script widevnum */
    char wnum_buf[MAX_INPUT_LENGTH];
    strlcpy(wnum_buf, script_str, sizeof(wnum_buf));

    WNUM script_wnum;
    AREA_DATA *context = pObj->area;
    if (!parse_widevnum(wnum_buf, context, &script_wnum)) {
        snprintf(errbuf, errlen, "Invalid script widevnum: %s", script_str);
        return false;
    }

    json_t *val = json_pack("{s:I, s:I, s:i, s:s, s:s}",
        "script_auid",  (json_int_t)script_wnum.pArea->uid,
        "script_vnum",  (json_int_t)script_wnum.vnum,
        "trigger_index", tindex,
        "trigger_name",  trigger_name(tindex),
        "phrase",        phrase ? phrase : "*");

    olc_stage_list_add(cs, "oprogs", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 7. addquest
 * ========================================================================= */

static json_t *oedit_action_quest_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "quest", "label", "Quest", "type", "widevnum",
        "required", true));

    return fields;
}

static bool oedit_action_quest_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *quest_str = json_string_value(json_object_get(values, "quest"));

    if (!quest_str) {
        snprintf(errbuf, errlen, "Missing required field: quest");
        return false;
    }

    char wnum_buf[MAX_INPUT_LENGTH];
    strlcpy(wnum_buf, quest_str, sizeof(wnum_buf));

    WNUM wnum;
    AREA_DATA *context = pObj->area;
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
 * 8. addwaypoint
 * ========================================================================= */

static json_t *oedit_action_waypoint_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "area", "label", "Area UID", "type", "int",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "x", "label", "X", "type", "int",
        "required", true));

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "y", "label", "Y", "type", "int",
        "required", true));

    return fields;
}

static bool oedit_action_waypoint_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    (void)entity;
    json_t *j_area = json_object_get(values, "area");
    json_t *j_x    = json_object_get(values, "x");
    json_t *j_y    = json_object_get(values, "y");

    if (!j_area || !j_x || !j_y) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    json_t *val = json_pack("{s:I, s:i, s:i}",
        "area", (json_int_t)json_integer_value(j_area),
        "x",    (int)json_integer_value(j_x),
        "y",    (int)json_integer_value(j_y));

    olc_stage_list_add(cs, "waypoints", val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 9. addtype
 * ========================================================================= */

static json_t *oedit_action_addtype_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    (void)entity;
    json_t *fields = json_array();

    json_t *type_opts = olc_flag_options_json(type_flags);
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:o}",
        "field", "type", "label", "Type", "type", "enum",
        "required", true, "options", type_opts));

    return fields;
}

static bool oedit_action_addtype_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    (void)entity;
    const char *type_str = json_string_value(json_object_get(values, "type"));

    if (!type_str) {
        snprintf(errbuf, errlen, "Missing required field: type");
        return false;
    }

    int value = flag_value(type_flags, type_str);
    if (value == NO_FLAG) {
        snprintf(errbuf, errlen, "Invalid type: %s", type_str);
        return false;
    }

    char path[MAX_INPUT_LENGTH];
    snprintf(path, sizeof(path), "typedata/+%s",
        flag_string(type_flags, value));

    json_t *val = json_true();
    olc_changeset_add_change(cs, path, OLC_FIELD_TYPE_DATA, json_null(), val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * 10. removetype
 * ========================================================================= */

static json_t *oedit_action_removetype_form(void *entity, CHAR_DATA *ch)
{
    (void)ch;
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    json_t *fields = json_array();

    /* Enumerate active secondary types on the object */
    json_t *type_opts = json_array();
    for (int i = 0; i < ITEM__MAX; i++) {
        if (i == pObj->item_type)
            continue;
        if (TBIT_TST(pObj->type_flags, i)) {
            const char *name = flag_string(type_flags, i);
            if (name && name[0])
                json_array_append_new(type_opts, json_string(name));
        }
    }

    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:o}",
        "field", "type", "label", "Type", "type", "enum",
        "required", true, "options", type_opts));

    return fields;
}

static bool oedit_action_removetype_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    (void)entity;
    const char *type_str = json_string_value(json_object_get(values, "type"));

    if (!type_str) {
        snprintf(errbuf, errlen, "Missing required field: type");
        return false;
    }

    int value = flag_value(type_flags, type_str);
    if (value == NO_FLAG) {
        snprintf(errbuf, errlen, "Invalid type: %s", type_str);
        return false;
    }

    char path[MAX_INPUT_LENGTH];
    snprintf(path, sizeof(path), "typedata/-%s",
        flag_string(type_flags, value));

    json_t *val = json_true();
    olc_changeset_add_change(cs, path, OLC_FIELD_TYPE_DATA, json_null(), val);
    json_decref(val);
    return true;
}

/* =========================================================================
 * Handler Table and Registration
 * ========================================================================= */

static const olc_action_handler_t oedit_actions[] = {
    { "addaffect",    ED_OBJECT, "inline", "Add Affect",      "affects",    oedit_action_affect_form,     oedit_action_affect_stage },
    { "addimmune",    ED_OBJECT, "inline", "Add Immunity",    "affects",    oedit_action_immune_form,     oedit_action_immune_stage },
    { "addspell",     ED_OBJECT, "inline", "Add Spell",       "spells",     oedit_action_spell_form,      oedit_action_spell_stage },
    { "addskill",     ED_OBJECT, "inline", "Add Skill",       "affects",    oedit_action_skill_form,      oedit_action_skill_stage },
    { "addcatalyst",  ED_OBJECT, "inline", "Add Catalyst",    "catalysts",  oedit_action_catalyst_form,   oedit_action_catalyst_stage },
    { "addoprog",     ED_OBJECT, "inline", "Add OProg",       "oprogs",     oedit_action_oprog_form,      oedit_action_oprog_stage },
    { "addquest",     ED_OBJECT, "inline", "Add Quest",       "quests",     oedit_action_quest_form,      oedit_action_quest_stage },
    { "addwaypoint",  ED_OBJECT, "inline", "Add Waypoint",    "waypoints",  oedit_action_waypoint_form,   oedit_action_waypoint_stage },
    { "addtype",      ED_OBJECT, "inline", "Add Type",        "typedata",   oedit_action_addtype_form,    oedit_action_addtype_stage },
    { "removetype",   ED_OBJECT, "inline", "Remove Type",     "typedata",   oedit_action_removetype_form, oedit_action_removetype_stage },
    { NULL }
};

void oedit_register_actions(void)
{
    olc_register_actions(oedit_actions);
}

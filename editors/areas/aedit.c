/***************************************************************************
 *  aedit.c - OLC Area Editor                                              *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework (Phase 3).                *
 ***************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"
#include "../common/olc_field_handlers.h"
#include "../common/olc_staged.h"
#include "../common/olc_changeset.h"

/***************************************************************************
 * Framework Helpers                                                       *
 ***************************************************************************/

static AREA_DATA *aedit_get_area(void *pEdit)
{
    return (AREA_DATA *)pEdit;
}

static void aedit_rebuild_auto_tags(AREA_DATA *pArea)
{
    if (!pArea)
        return;

    free_string(pArea->auto_tags);
    pArea->auto_tags = short_to_name(pArea->name);
}

static AREA_DATA *aedit_find_area_by_name(const char *name)
{
    AREA_DATA *area;

    if (IS_NULLSTR(name))
        return NULL;

    for (area = area_first; area != NULL; area = area->next)
    {
        if (!IS_NULLSTR(area->name) && !str_cmp(area->name, name))
            return area;
    }

    return NULL;
}

static void aedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void aedit_show_regions_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void aedit_show_dependencies_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void aedit_show_scripts_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

/***************************************************************************
 * Area Editor Command Table (moved from olc.c)                            *
 ***************************************************************************/

const struct olc_cmd_type aedit_table[] =
{
    {   "?",            show_help           },
    {   "addaprog",     aedit_addaprog      },
    {   "addtrade",     aedit_add_trade     },
    {   "age",          aedit_age           },
    {   "airshipland",  aedit_airshipland   },
    {   "areawho",      aedit_areawho       },
    {   "builder",      aedit_builder       },
    {   "commands",     show_commands       },
    {   "comments",     aedit_comments      },
    {   "create",       aedit_create        },
    {   "credits",      aedit_credits       },
    {   "delaprog",     aedit_delaprog      },
    {   "description",  aedit_desc          },
    {   "filename",     aedit_file          },
    {   "flags",        aedit_flags         },
    {   "landx",        aedit_land_x        },
    {   "landy",        aedit_land_y        },
    {   "levels",       aedit_levels        },
    {   "name",         aedit_name          },
    {   "notes",        aedit_notes         },
    {   "open",         aedit_open          },
    {   "placetype",    aedit_placetype     },
    {   "postoffice",   aedit_postoffice    },
    {   "recall",       aedit_recall        },
    {   "removetrade",  aedit_remove_trade  },
    {   "repop",        aedit_repop         },
    {   "regions",      aedit_regions       },
    {   "security",     aedit_security      },
    {   "settrade",     aedit_set_trade     },
    {   "show",         aedit_show          },
    {   "tags",         aedit_tags          },
    {   "topic",        aedit_topic         },
    {   "varclear",     aedit_varclear      },
    {   "varset",       aedit_varset        },
    {   "viewtrade",    aedit_view_trade    },
    {   "vnum",         aedit_vnum          },
    {   "wilds",        aedit_wilds         },
    {   "x",            aedit_x             },
    {   "y",            aedit_y             },
    {   NULL,           0                   }
};

/***************************************************************************
 * Field Handler Table                                                     *
 ***************************************************************************/

OLC_FIELD_APPLY_FLAGS (aedit_apply_area_flags,  AREA_DATA, area_flags)
OLC_FIELD_APPLY_INT   (aedit_apply_x,           AREA_DATA, x)
OLC_FIELD_APPLY_INT   (aedit_apply_y,           AREA_DATA, y)
OLC_FIELD_APPLY_INT   (aedit_apply_land_x,      AREA_DATA, land_x)
OLC_FIELD_APPLY_INT   (aedit_apply_land_y,      AREA_DATA, land_y)
OLC_FIELD_APPLY_BOOL  (aedit_apply_open,        AREA_DATA, open)
OLC_FIELD_APPLY_STRING(aedit_apply_name,        AREA_DATA, name)
OLC_FIELD_APPLY_STRING(aedit_apply_tags,        AREA_DATA, tags)
OLC_FIELD_APPLY_STRING(aedit_apply_description, AREA_DATA, description)
OLC_FIELD_APPLY_STRING(aedit_apply_comments,    AREA_DATA, comments)
OLC_FIELD_APPLY_STRING(aedit_apply_notes,       AREA_DATA, notes)
OLC_FIELD_APPLY_INT   (aedit_apply_repop,       AREA_DATA, repop)
OLC_FIELD_APPLY_STRING(aedit_apply_credits,     AREA_DATA, credits)
OLC_FIELD_APPLY_INT16 (aedit_apply_age,         AREA_DATA, age)
OLC_FIELD_APPLY_INT   (aedit_apply_security,    AREA_DATA, security)
OLC_FIELD_APPLY_STRING(aedit_apply_topic,        AREA_DATA, area_topic)
OLC_FIELD_APPLY_LONG  (aedit_apply_wilds,        AREA_DATA, wilds_uid)
OLC_FIELD_APPLY_INT   (aedit_apply_areawho,      AREA_DATA, area_who)
OLC_FIELD_APPLY_LONG  (aedit_apply_placetype,    AREA_DATA, place_flags)
OLC_FIELD_APPLY_STRING(aedit_apply_file,         AREA_DATA, file_name)
OLC_FIELD_APPLY_LONG  (aedit_apply_min_vnum,     AREA_DATA, min_vnum)
OLC_FIELD_APPLY_LONG  (aedit_apply_max_vnum,     AREA_DATA, max_vnum)
OLC_FIELD_APPLY_INT16 (aedit_apply_min_level,    AREA_DATA, min_level)
OLC_FIELD_APPLY_INT16 (aedit_apply_max_level,    AREA_DATA, max_level)

static bool aedit_apply_recall(void *entity, olc_pending_change_t *change) {
    AREA_DATA *pArea = (AREA_DATA *)entity;
    const char *val = json_string_value(change->new_value);

    if (IS_NULLSTR(val) || !str_cmp(val, "0")) {
        location_clear(&pArea->recall);
        return true;
    }

    if (!str_prefix("room ", val)) {
        long vnum = atol(val + 5);
        location_set(&pArea->recall, 0, vnum, 0, 0);
        return true;
    }

    if (!str_prefix("wilds ", val)) {
        unsigned long wuid, x, y, z;
        if (sscanf(val, "wilds %lu %lu %lu %lu", &wuid, &x, &y, &z) == 4) {
            location_set(&pArea->recall, wuid, x, y, z);
            return true;
        }
    }

    return false;
}

static bool aedit_apply_airshipland(void *entity, olc_pending_change_t *change) {
    AREA_DATA *pArea = (AREA_DATA *)entity;
    const char *val = json_string_value(change->new_value);

    if (IS_NULLSTR(val) || !str_cmp(val, "0") || !str_cmp(val, "none") || !str_cmp(val, "clear")) {
        pArea->airship_land_load.auid = 0;
        pArea->airship_land_load.vnum = 0;
        pArea->airship_land_wnum.pArea = NULL;
        pArea->airship_land_wnum.vnum = 0;
        return true;
    }

    WNUM room_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(pArea, val);
    char buf[MAX_INPUT_LENGTH];
    strlcpy(buf, val, sizeof(buf));
    if (!parse_widevnum(buf, context, &room_wnum)) return false;

    ROOM_INDEX_DATA *room = get_room_index(room_wnum.pArea, room_wnum.vnum);
    if (!room) return false;

    pArea->airship_land_load.auid = room_wnum.pArea->uid;
    pArea->airship_land_load.vnum = room_wnum.vnum;
    pArea->airship_land_wnum = room_wnum;
    return true;
}

static bool aedit_apply_postoffice(void *entity, olc_pending_change_t *change) {
    AREA_DATA *pArea = (AREA_DATA *)entity;
    const char *val = json_string_value(change->new_value);

    if (IS_NULLSTR(val) || !str_cmp(val, "0") || !str_cmp(val, "none") || !str_cmp(val, "clear")) {
        pArea->post_office_load.auid = 0;
        pArea->post_office_load.vnum = 0;
        pArea->post_office_wnum.pArea = NULL;
        pArea->post_office_wnum.vnum = 0;
        return true;
    }

    WNUM room_wnum;
    char buf[MAX_INPUT_LENGTH];
    strlcpy(buf, val, sizeof(buf));
    if (!parse_widevnum(buf, pArea, &room_wnum) || room_wnum.pArea != pArea) return false;

    ROOM_INDEX_DATA *room = get_room_index(room_wnum.pArea, room_wnum.vnum);
    if (!room) return false;

    pArea->post_office_load.auid = pArea->uid;
    pArea->post_office_load.vnum = room_wnum.vnum;
    pArea->post_office_wnum = room_wnum;
    return true;
}

static bool aedit_apply_var(void *entity, olc_pending_change_t *change) {
    AREA_DATA *pArea = (AREA_DATA *)entity;

    if (json_is_null(change->new_value)) {
        const char *varname = change->field_path + 4; /* skip "var/" */
        char buf[MAX_INPUT_LENGTH];
        strlcpy(buf, varname, sizeof(buf));
        olc_varclear(&pArea->index_vars, NULL, buf, true);
        if (pArea->progs)
            olc_varclear(&pArea->progs->vars, NULL, buf, true);
        return true;
    } else {
        const char *arg = json_string_value(change->new_value);
        if (!arg) return false;
        char buf[MAX_INPUT_LENGTH];
        strlcpy(buf, arg, sizeof(buf));
        olc_varset(&pArea->index_vars, NULL, buf, true);
        if (pArea->progs)
            olc_varset(&pArea->progs->vars, NULL, buf, true);
        return true;
    }
}

static bool aedit_apply_aprog_ops(void *entity, olc_pending_change_t *change)
{
    AREA_DATA *pArea = (AREA_DATA *)entity;

    if (change->field_type == OLC_FIELD_LIST_REMOVE) {
        if (!pArea->progs || !pArea->progs->progs) return false;

        int group_index = (int)json_integer_value(
            json_object_get(change->new_value, "group_index"));
        json_t *jtrig = json_object_get(change->new_value, "trigger_index");
        int trigger_idx = jtrig ? (int)json_integer_value(jtrig) : 0;

        PROG_GROUP groups[MAX_PROG_GROUPS];
        int num_groups = prog_build_groups(pArea->progs->progs, groups,
            MAX_PROG_GROUPS, PRG_APROG);

        if (group_index < 1 || group_index > num_groups) return false;
        PROG_GROUP *group = &groups[group_index - 1];

        if (trigger_idx > 0) {
            if (trigger_idx > group->trigger_count) return false;
            PROG_GROUP_ENTRY *entry = &group->triggers[trigger_idx - 1];
            return edit_deltrigger_specific(pArea->progs->progs, group->script,
                entry->entry->trig_type, entry->entry->trig_phrase);
        } else {
            return edit_delscript(pArea->progs->progs, group->script);
        }
    }

    if (change->field_type == OLC_FIELD_LIST_ADD) {
        long auid = (long)json_integer_value(
            json_object_get(change->new_value, "script_auid"));
        long vnum = (long)json_integer_value(
            json_object_get(change->new_value, "script_vnum"));
        int tindex = (int)json_integer_value(
            json_object_get(change->new_value, "trigger_index"));
        const char *phrase = json_string_value(
            json_object_get(change->new_value, "phrase"));
        if (!phrase) return false;

        AREA_DATA *area = get_area_from_uid(auid);
        if (!area) return false;

        SCRIPT_DATA *code = get_script_index(area, vnum, PRG_APROG);
        if (!code) return false;

        if (!pArea->progs) return false;
        if (!pArea->progs->progs) pArea->progs->progs = new_prog_bank();

        int slot = trigger_table[tindex].slot;

        PROG_LIST *list = new_trigger();
        list->vnum = vnum;
        list->script_is_widevnum = true;
        list->script_load.auid = auid;
        list->script_load.vnum = vnum;
        list->trig_type = tindex;
        list->trig_phrase = str_dup(phrase);
        if (is_widevnum_format(phrase)) {
            list->numeric = true;
            list->trig_is_widevnum = true;
            parse_widevnum_load(phrase, &list->trig_load);
            list->trig_number = (int)list->trig_load.vnum;
        } else {
            list->trig_number = atoi(list->trig_phrase);
            list->numeric = is_number(list->trig_phrase);
        }
        list->script = code;

        list_appendlink(pArea->progs->progs[slot], list);
        return true;
    }

    return false;
}

static bool aedit_apply_trade_ops(void *entity, olc_pending_change_t *change)
{
    AREA_DATA *pArea = (AREA_DATA *)entity;

    if (change->field_type == OLC_FIELD_LIST_REMOVE) {
        int index = (int)json_integer_value(
            json_object_get(change->new_value, "index"));

        TRADE_ITEM *prev = NULL;
        int count = 0;
        for (TRADE_ITEM *t = pArea->trade_list; t; t = t->next) {
            if (count == index) {
                if (prev) prev->next = t->next;
                else pArea->trade_list = t->next;
                free_trade_item(t);
                return true;
            }
            count++;
            prev = t;
        }
        return false;
    }

    if (change->field_type == OLC_FIELD_LIST_ADD) {
        long obj_auid = (long)json_integer_value(
            json_object_get(change->new_value, "obj_auid"));
        long obj_vnum = (long)json_integer_value(
            json_object_get(change->new_value, "obj_vnum"));
        int trade_type = (int)json_integer_value(
            json_object_get(change->new_value, "trade_type"));
        long rep_time = (long)json_integer_value(
            json_object_get(change->new_value, "replenish_time"));
        long rep_amt = (long)json_integer_value(
            json_object_get(change->new_value, "replenish_amount"));
        long max_qty = (long)json_integer_value(
            json_object_get(change->new_value, "max_qty"));
        long min_price = (long)json_integer_value(
            json_object_get(change->new_value, "min_price"));
        long max_price = (long)json_integer_value(
            json_object_get(change->new_value, "max_price"));

        AREA_DATA *area = get_area_from_uid(obj_auid);
        if (!area) return false;

        OBJ_INDEX_DATA *pObj = get_obj_index(area, obj_vnum);
        if (!pObj) return false;

        new_trade_item(pArea, (int16_t)trade_type, rep_time, rep_amt,
            max_qty, min_price, max_price, obj_vnum);
        return true;
    }

    return false;
}

static const olc_field_handler_t aedit_field_handlers[] = {
    { "Area Flags",        OLC_FIELD_FLAGS,     NULL, aedit_apply_area_flags,  NULL },
    { "X Coordinate",      OLC_FIELD_INT,       NULL, aedit_apply_x,           NULL },
    { "Y Coordinate",      OLC_FIELD_INT,       NULL, aedit_apply_y,           NULL },
    { "Land X Coordinate", OLC_FIELD_INT,       NULL, aedit_apply_land_x,      NULL },
    { "Land Y Coordinate", OLC_FIELD_INT,       NULL, aedit_apply_land_y,      NULL },
    { "Open",              OLC_FIELD_BOOL,      NULL, aedit_apply_open,        NULL },
    { "Name",              OLC_FIELD_STRING,     NULL, aedit_apply_name,        NULL },
    { "Tags",              OLC_FIELD_STRING,     NULL, aedit_apply_tags,        NULL },
    { "Description",       OLC_FIELD_MULTILINE,  NULL, aedit_apply_description, NULL },
    { "Comments",          OLC_FIELD_MULTILINE,  NULL, aedit_apply_comments,    NULL },
    { "Notes",             OLC_FIELD_MULTILINE,  NULL, aedit_apply_notes,       NULL },
    { "Repop Time",        OLC_FIELD_INT,       NULL, aedit_apply_repop,       NULL },
    { "Credits",           OLC_FIELD_STRING,     NULL, aedit_apply_credits,     NULL },
    { "Age",               OLC_FIELD_INT16,      NULL, aedit_apply_age,         NULL },
    { "Security",          OLC_FIELD_INT,        NULL, aedit_apply_security,    NULL },
    { "Topic",             OLC_FIELD_STRING,     NULL, aedit_apply_topic,       NULL },
    { "Wilderness",        OLC_FIELD_LONG,       NULL, aedit_apply_wilds,       NULL },
    { "Area Who",          OLC_FIELD_INT,        NULL, aedit_apply_areawho,     NULL },
    { "Place Type",        OLC_FIELD_LONG,       NULL, aedit_apply_placetype,   NULL },
    { "File Name",         OLC_FIELD_STRING,     NULL, aedit_apply_file,        NULL },
    { "Recall",            OLC_FIELD_STRING,     NULL, aedit_apply_recall,      NULL },
    { "Airship Land",      OLC_FIELD_STRING,     NULL, aedit_apply_airshipland, NULL },
    { "Post Office",       OLC_FIELD_STRING,     NULL, aedit_apply_postoffice,  NULL },
    { "Min Vnum",          OLC_FIELD_LONG,       NULL, aedit_apply_min_vnum,    NULL },
    { "Max Vnum",          OLC_FIELD_LONG,       NULL, aedit_apply_max_vnum,    NULL },
    { "Min Level",         OLC_FIELD_INT16,      NULL, aedit_apply_min_level,   NULL },
    { "Max Level",         OLC_FIELD_INT16,      NULL, aedit_apply_max_level,   NULL },
    { "var/*",             OLC_FIELD_STRING,     NULL, aedit_apply_var,         NULL },
    { "aprogs/**",         OLC_FIELD_LIST_ADD,   NULL, aedit_apply_aprog_ops,   NULL },
    { "trades/**",         OLC_FIELD_LIST_ADD,   NULL, aedit_apply_trade_ops,   NULL },
    { NULL, 0, NULL, NULL, NULL }
};

/***************************************************************************
 * Editor Definition (Unified Framework)                                   *
 ***************************************************************************/

static const olc_field_annotation_t aedit_annotations[] = {
    { "Repop Time", .min = 5,   .max = 120 },
    { "Age",        .min = 0,   .max = INT16_MAX },
    { NULL }
};

static const OLC_EDITOR_DEF aedit_def = {
    .name           = "AEdit",
    .editor_type    = ED_AREA,
    .cmd_table      = aedit_table,
    .show_fn        = aedit_show,
    .tabs           = {
        .count = 4,
        .tabs = {
            { "General", "Gen", aedit_show_general_tab },
            { "Regions", "Reg", aedit_show_regions_tab },
            { "Dependencies", "Dep", aedit_show_dependencies_tab },
            { "Scripts", "Scr", aedit_show_scripts_tab },
        }
    },
    .theme          = &olc_theme_world,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR
    },
    .change_mode    = OLC_CHANGE_STAGED,
    .get_area_fn    = aedit_get_area,
    .audit_changes  = true,
    .field_handlers = aedit_field_handlers,
    .annotations    = aedit_annotations,
};

/***************************************************************************
 * Editor Entry Point (moved from olc.c)                                   *
 ***************************************************************************/

/**
 * do_aedit - Enter the area editor
 *
 * Usage: aedit                - edit current area
 *        aedit <uid>          - edit area by UID
 *        aedit <keyword>      - edit area by keyword
 *        aedit create         - create new area
 */
extern void aedit_register_actions(void);

void do_aedit(CHAR_DATA *ch, char *argument)
{
    static bool actions_registered = false;
    if (!actions_registered) {
        aedit_register_actions();
        actions_registered = true;
    }

    AREA_DATA *pArea;
    int value;
    char arg[MAX_STRING_LENGTH];

    if (!olc_editor_check_perm(ch, &aedit_def, NULL)) {
        send_to_char("AEdit: Insufficient security to edit areas.\n\r", ch);
        return;
    }

    if (IS_NPC(ch))
        return;

    pArea = ch->in_room->area;
    argument = one_argument(argument, arg);

    if (is_number(arg))
    {
        value = atoi(arg);
        pArea = get_area_from_uid(value);

        if (!pArea)
        {
            send_to_char("That area UID does not exist.\n\r", ch);
            return;
        }
    }
    else if (arg[0] != '\0' && (pArea = find_area_kwd(arg)) == NULL
        && str_cmp(arg, "create"))
    {
        send_to_char("Area not found.\n\r", ch);
        return;
    }
    else if (!str_cmp(arg, "create"))
    {
        if (ch->pcdata->security < 9 || get_staff_rank(ch) < STAFF_CREATOR)
        {
            send_to_char("AEdit: Insufficient security to create areas.\n\r", ch);
            return;
        }

        if (aedit_create(ch, ""))
            olc_editor_enter(ch, &aedit_def, ch->desc->pEdit, false);
        return;
    }

    olc_editor_enter(ch, &aedit_def, (void *)pArea, false);
}

void do_ashow(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *pArea;
    char arg[MAX_STRING_LENGTH];
    char *name_lookup;

    if (!olc_editor_check_perm(ch, &aedit_def, NULL)) {
        send_to_char("AShow: Insufficient security to view area details.\n\r", ch);
        return;
    }

    if (IS_NPC(ch))
        return;

    name_lookup = argument;
    while (name_lookup[0] != '\0' && isspace((unsigned char)name_lookup[0]))
        name_lookup++;

    argument = one_argument(argument, arg);
    pArea = ch->in_room ? ch->in_room->area : NULL;

    if (!IS_NULLSTR(arg)) {
        if (is_number(arg)) {
            pArea = get_area_from_uid(atol(arg));
        } else {
            pArea = find_area_kwd(arg);
        }

        if (!pArea)
            pArea = aedit_find_area_by_name(name_lookup);
    }

    if (!pArea) {
        send_to_char("Area not found.\n\r", ch);
        return;
    }

    olc_show_item(ch, (void *)pArea, aedit_show, "");
}

/***************************************************************************
 * Editor Interpreter (Framework)                                          *
 ***************************************************************************/

/**
 * aedit - Interpreter loop for the area editor
 */
void aedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &aedit_def);
}

/***************************************************************************
 * Display                                                                 *
 ***************************************************************************/

static void aedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    AREA_DATA *pArea = (AREA_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&aedit_def);
    ROOM_INDEX_DATA *recall;
    const char *file_format;
    const char *dot;

    olc_display_string(ctx, theme, "Name:", "name", pArea->name);
    olc_display_string(ctx, theme, "Tags:", "tags", IS_NULLSTR(pArea->tags) ? "(none)" : pArea->tags);
    olc_display_string(ctx, theme, "Auto Tags:", NULL, IS_NULLSTR(pArea->auto_tags) ? "(none)" : pArea->auto_tags);
    olc_display_string(ctx, theme, "Topic:", "topic", IS_NULLSTR(pArea->area_topic) ? "(default)" : pArea->area_topic);
    olc_display_number(ctx, theme, "Area ID:", NULL, pArea->uid);

    olc_display_section(ctx, theme, "System Information");
    olc_display_string(ctx, theme, "File:", "filename", pArea->file_name);
    dot = pArea->file_name ? strrchr(pArea->file_name, '.') : NULL;
    if (dot && !str_cmp(dot, ".json"))
        file_format = "JSON";
    else if (dot && !str_cmp(dot, ".are"))
        file_format = "Legacy (.are)";
    else
        file_format = "Unknown";
    olc_display_string(ctx, theme, "Storage:", NULL, file_format);
    olc_display_pair(ctx, theme,
        "Age:", NULL, formatf("%d", pArea->age),
        "Repop:", "repop", formatf("%d minutes", pArea->repop));
    olc_display_number(ctx, theme, "Players:", NULL, pArea->nplayer);
    olc_display_string(ctx, theme, "Credits:", "credits", pArea->credits);
    olc_display_flags(ctx, theme, "Flags:", "flags", area_flags, pArea->area_flags);
    olc_display_bool(ctx, theme, "Open:", "open", pArea->open);

    olc_display_section(ctx, theme, "OLC Info");
    {
        long display_min = 0;
        long display_max = 0;
        if (pArea->room_list && list_size(pArea->room_list) > 0) {
            ITERATOR it;
            ROOM_INDEX_DATA *room_iter;

            iterator_start(&it, pArea->room_list);
            while ((room_iter = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) != NULL) {
                if (room_iter->vnum <= 0)
                    continue;

                if (display_min == 0 || room_iter->vnum < display_min)
                    display_min = room_iter->vnum;
                if (display_max == 0 || room_iter->vnum > display_max)
                    display_max = room_iter->vnum;
            }
            iterator_stop(&it);
        }

        if (display_min == 0 || display_max == 0) {
            display_min = pArea->min_vnum;
            display_max = pArea->max_vnum;
        }

        olc_display_pair(ctx, theme,
            "Min Vnum:", NULL, formatf("%ld", display_min),
            "Max Vnum:", NULL, formatf("%ld", display_max));
    }
    olc_display_number(ctx, theme, "Security:", "security", pArea->security);
    olc_display_string(ctx, theme, "Builders:", "builder", pArea->builders);
    olc_display_pair(ctx, theme,
        "Min Level:", NULL, formatf("%d", pArea->min_level),
        "Max Level:", NULL, formatf("%d", pArea->max_level));

    olc_display_section(ctx, theme, "Location Information");
    {
        char recall_buf[MSL];
        if (pArea->recall.wuid) {
            WILDS_DATA *wilds = get_wilds_from_uid(NULL, pArea->recall.wuid);
            if (wilds)
                sprintf(recall_buf, "Wilds %s [%lu] at <%lu,%lu,%lu>",
                    wilds->name, pArea->recall.wuid,
                    pArea->recall.id[0], pArea->recall.id[1], pArea->recall.id[2]);
            else
                sprintf(recall_buf, "Wilds ??? [%lu]", pArea->recall.wuid);
        } else if (pArea->recall.id[0] > 0
            && (recall = get_room_index(pArea, pArea->recall.id[0]))) {
            sprintf(recall_buf, "Room [%s] %s", widevnum_string_room(recall, pArea), recall->name);
        } else {
            sprintf(recall_buf, "(none)");
        }
        olc_display_string(ctx, theme, "Recall:", "recall", recall_buf);
    }

    {
        ROOM_INDEX_DATA *landing = get_room_index(pArea, pArea->airship_land_load.vnum);
        olc_display_widevnum(ctx, theme, "AirshipLand:", "airshipland",
            landing ? widevnum_string_room(landing, pArea) :
                (pArea->airship_land_load.vnum > 0 ? formatf("%ld", pArea->airship_land_load.vnum) : NULL),
            landing ? landing->name : NULL);
    }

    olc_display_section(ctx, theme, "Wilderness Map Locations");
    if (pArea->wilds_uid > 0) {
        WILDS_DATA *pWilds = get_wilds_from_uid(NULL, pArea->wilds_uid);
        olc_display_string(ctx, theme, "Wilderness:", "wilds",
            formatf("[%ld] %s", pArea->wilds_uid, pWilds ? pWilds->name : "(null)"));
    } else {
        olc_display_string(ctx, theme, "Wilderness:", "wilds", "(none)");
    }

    olc_display_pair(ctx, theme,
        "X:", "x", formatf("%d", pArea->x),
        "Y:", "y", formatf("%d", pArea->y));
    olc_display_pair(ctx, theme,
        "LandX:", "landx", formatf("%d", pArea->land_x),
        "LandY:", "landy", formatf("%d", pArea->land_y));

    if (pArea->trade_list != NULL) {
        TRADE_ITEM *temp;
        olc_display_section(ctx, theme, "Trade Items");

        OLC_TABLE_COL trade_cols[] = {
            { "Name",       18, false },
            { "Obj Vnum",    8, true  },
            { "Rep.Time",    8, true  },
            { "Rep.Amt",     8, true  },
            { "Max Qty",     8, true  },
            { "Min Price",   8, true  },
            { "Max Price",   8, true  },
        };
        olc_display_table_begin(ctx, theme, NULL, trade_cols, 7);

        for (temp = pArea->trade_list; temp != NULL; temp = temp->next) {
            OBJ_INDEX_DATA *trade_obj = get_obj_index(pArea, temp->obj_load.vnum);
            const char *vals[7] = {
                trade_table[temp->trade_type].name,
                trade_obj ? widevnum_string_object(trade_obj, pArea)
                          : formatf("%ld", temp->obj_load.vnum),
                formatf("%ld", temp->replenish_time),
                formatf("%ld", temp->replenish_amount),
                formatf("%ld", temp->max_qty),
                formatf("%ld", temp->min_price),
                formatf("%ld", temp->max_price),
            };
            olc_display_table_row(ctx, theme, vals, 7, false);
        }
        olc_display_table_end(ctx, theme);
    }

    olc_display_text(ctx, theme, "Description:", "description", pArea->description);
    olc_display_text(ctx, theme, "Player Notes:", "notes", pArea->notes);
    olc_display_text(ctx, theme, "Builders' Comments:", "comments", pArea->comments);
}

static void aedit_show_regions_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    AREA_DATA *pArea = (AREA_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&aedit_def);

    olc_display_type(ctx, theme, "AreaWho:", "areawho", area_who_titles, pArea->area_who);
    olc_display_type(ctx, theme, "PlaceType:", "placetype", place_flags, pArea->place_flags);

    olc_display_section(ctx, theme, "Region Defaults");
    olc_display_type(ctx, theme, "Def Who:", "regions who default", area_who_titles, pArea->region.area_who);
    olc_display_type(ctx, theme, "Def Place:", "regions place default", place_flags, pArea->region.rs_place_flags);
    olc_display_string(ctx, theme, "Def Flags:", "regions flags default",
        flag_string(area_region_flags, pArea->region.flags));
    olc_display_infof(ctx, theme, "Custom regions:", "%zu", pArea->regions ? list_size(pArea->regions) : 0);

    if (pArea->regions && list_size(pArea->regions) > 0) {
        ITERATOR it;
        AREA_REGION *region;
        OLC_TABLE_COL region_cols[] = {
            { "UID", 8, true },
            { "Name", 24, false },
            { "Who", 14, false },
            { "Place", 14, false },
        };

        olc_display_table_begin(ctx, theme, NULL, region_cols, 4);
        iterator_start(&it, pArea->regions);
        while ((region = (AREA_REGION *)iterator_nextdata(&it)) != NULL) {
            const char *vals[4] = {
                formatf("%ld", region->uid),
                IS_NULLSTR(region->name) ? "(unnamed)" : region->name,
                flag_string(area_who_titles, region->area_who),
                flag_string(place_flags, region->rs_place_flags),
            };
            olc_display_table_row(ctx, theme, vals, 4, false);
        }
        iterator_stop(&it);
        olc_display_table_end(ctx, theme);
    }
}

static void aedit_show_dependencies_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    AREA_DATA *pArea = (AREA_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&aedit_def);
    AREA_DEPENDENCY *dependency;

    olc_display_section(ctx, theme, "Dependencies");
    olc_display_infof(ctx, theme, "References:", "%ld", pArea->dependency_count);
    olc_display_infof(ctx, theme, "Known gap:",
        "Runtime-created script refs are not statically discoverable");

    if (!pArea->dependencies) {
        olc_display_infof(ctx, theme, "(none)");
        return;
    }

    {
        OLC_TABLE_COL dep_cols[] = {
            { "How",        16, false },
            { "Source",     30, false },
            { "Target",     40, false },
        };

        olc_display_table_begin(ctx, theme, NULL, dep_cols, 3);

        for (dependency = pArea->dependencies; dependency != NULL; dependency = dependency->next) {
            const char *vals[3] = {
                dependency->reference_type,
                formatf("%s %ld#%ld %s",
                    dependency->source_type,
                    pArea->uid,
                    dependency->source_vnum,
                    IS_NULLSTR(dependency->source_name) ? "" : dependency->source_name),
                formatf("%s :: %s %ld#%ld %s",
                    IS_NULLSTR(dependency->target_area_name) ? "(unknown area)" : dependency->target_area_name,
                    IS_NULLSTR(dependency->target_type) ? "entity" : dependency->target_type,
                    dependency->target_area_uid,
                    dependency->target_vnum,
                    IS_NULLSTR(dependency->target_name) ? "" : dependency->target_name),
            };

            olc_display_table_row(ctx, theme, vals, 3, false);
        }

        olc_display_table_end(ctx, theme);
    }
}

static void aedit_show_scripts_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    AREA_DATA *pArea = (AREA_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&aedit_def);

    if (pArea->progs && pArea->progs->progs)
        olc_display_scripts(ctx, theme, pArea->progs->progs, PRG_APROG,
            "AreaProg Vnum", "addaprog", "delaprog");

    olc_display_vars(ctx, theme, pArea->index_vars, "varset", "varclear");
}

AEDIT(aedit_show)
{
    AREA_DATA *pArea;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&aedit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    EDIT_AREA(ch, pArea);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "AEdit", pArea->name,
        formatf("%ld", pArea->uid), &aedit_def);

    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        for (int i = 0; i < aedit_def.tabs.count; i++) {
            if (aedit_def.tabs.tabs[i].show_fn)
                aedit_def.tabs.tabs[i].show_fn(ch, ctx, (void *)pArea);
        }
    } else if (tab >= 0 && tab < aedit_def.tabs.count && aedit_def.tabs.tabs[tab].show_fn) {
        aedit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)pArea);
    } else {
        aedit_show_general_tab(ch, ctx, (void *)pArea);
    }

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}


AEDIT(aedit_flags)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_flag_toggle(ch, argument, "Area Flags",
        "Syntax:  flags [flag]\n\rType '? areaflags' for a list of flags.\n\r",
        &pArea->area_flags, area_flags, NULL, NULL);
}


AEDIT(aedit_x)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_number(ch, argument, "X Coordinate",
        "Syntax:  x [#x coord on map]\n\r",
        &pArea->x, INT_MIN, INT_MAX, NULL, NULL);
}


AEDIT(aedit_y)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_number(ch, argument, "Y Coordinate",
        "Syntax:  y [#y coord on map]\n\r",
        &pArea->y, INT_MIN, INT_MAX, NULL, NULL);
}


AEDIT(aedit_land_x)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_number(ch, argument, "Land X Coordinate",
        "Syntax:  landx [#x coord on map]\n\r",
        &pArea->land_x, INT_MIN, INT_MAX, NULL, NULL);
}


AEDIT(aedit_land_y)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_number(ch, argument, "Land Y Coordinate",
        "Syntax:  landy [#y coord on map]\n\r",
        &pArea->land_y, INT_MIN, INT_MAX, NULL, NULL);
}

AEDIT(aedit_wilds)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (!is_number(argument))
    {
        send_to_char("Syntax:  wilds [map uid]\n\r", ch);
        return false;
    }

    long wuid = atol(argument);
    if (wuid != 0 && !get_wilds_from_uid(NULL, wuid))
    {
        send_to_char("Invalid wilds map.\n\r", ch);
        return false;
    }

    return olc_cmd_long(ch, argument, "Wilderness", NULL,
        &pArea->wilds_uid, 0, LONG_MAX, NULL, NULL);
}


AEDIT(aedit_airshipland)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  airshipland [widevnum]\n\r", ch);
        return false;
    }

    const char *stage_val = argument;
    WNUM room_wnum;
    ROOM_INDEX_DATA *pRoom = NULL;

    if (str_cmp(argument, "0") && str_cmp(argument, "none") && str_cmp(argument, "clear")) {
        AREA_DATA *context = olc_relative_widevnum_context(pArea, argument);
        if (!parse_widevnum(argument, context, &room_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        pRoom = get_room_index(room_wnum.pArea, room_wnum.vnum);
        if (!pRoom) {
            send_to_char("That room doesn't exist.\n\r", ch);
            return false;
        }
    }

    /* Staged mode */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            if (!olc_check_staging_limits(ch, cs)) return false;

            char old_buf[MAX_INPUT_LENGTH];
            if (pArea->airship_land_load.vnum > 0)
                snprintf(old_buf, sizeof(old_buf), "%ld#%ld",
                    pArea->airship_land_load.auid, pArea->airship_land_load.vnum);
            else
                strlcpy(old_buf, "none", sizeof(old_buf));

            json_t *old_val = json_string(old_buf);
            json_t *new_val = json_string(stage_val);
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, "Airship Land", OLC_FIELD_STRING, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);
            if (result)
                printf_to_char(ch, "{G[STAGED]{x Airship landing staged.\n\r");
            else
                send_to_char("Airship landing reverted to original value.\n\r", ch);
            return result != NULL;
        }
    }

    /* Non-staged fallback */
    if (!str_cmp(argument, "0") || !str_cmp(argument, "none") || !str_cmp(argument, "clear")) {
        pArea->airship_land_load.auid = 0;
        pArea->airship_land_load.vnum = 0;
        pArea->airship_land_wnum.pArea = NULL;
        pArea->airship_land_wnum.vnum = 0;
        send_to_char("Airship landing cleared.\n\r", ch);
        return true;
    }

    pArea->airship_land_load.auid = room_wnum.pArea->uid;
    pArea->airship_land_load.vnum = room_wnum.vnum;
    pArea->airship_land_wnum = room_wnum;
    printf_to_char(ch, "Set airship land spot of %s to %s - %s\n\r",
        pArea->name, widevnum_string_room(pRoom, pArea), pRoom->name);
    return true;
}

AEDIT( aedit_add_trade )
{
    AREA_DATA *pArea;
    OBJ_INDEX_DATA *pObj;

    char arg1[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char arg3[MAX_STRING_LENGTH];
    char arg4[MAX_STRING_LENGTH];
    char arg5[MAX_STRING_LENGTH];
    char arg6[MAX_STRING_LENGTH];

    long replenish_time;
    long replenish_amount;
    long max_qty;
    long min_price;
    long max_price;
    long obj_vnum;

    EDIT_AREA(ch, pArea);

    argument = one_argument( argument, arg1);
    argument = one_argument( argument, arg2);
    argument = one_argument( argument, arg3);
    argument = one_argument( argument, arg4);
    argument = one_argument( argument, arg5);
    argument = one_argument( argument, arg6);

    if ( arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0' || arg4[0] == '\0' ||
            arg5[0] == '\0' || arg6[0] == '\0' )
    {
    send_to_char("addtrade obj_vnum replenish_time replenish_amount max_qty min_price max_price\n\r", ch);
    return false;
    }

    obj_vnum = atoi( arg1 );
    replenish_time = atoi( arg2 );
    replenish_amount = atoi( arg3 );
    max_qty = atoi( arg4 );
    min_price = atoi( arg5 );
    max_price = atoi( arg6 );

    if ( ( pObj = get_obj_index( pArea, obj_vnum ) ) == NULL )
    {
        send_to_char( "That object does not exist!\n\r", ch );
        return false;
    }

    if ( pObj->value[0] == TRADE_NONE )
    {
        send_to_char( "That is not a valid trade item.\n\r", ch );
        return false;
    }

    new_trade_item(pArea, pObj->value[0], replenish_time, replenish_amount, max_qty, min_price, max_price, obj_vnum);
    send_to_char("Trade item added.\n\r", ch);

    return true;
}


AEDIT( aedit_set_trade)
{
    return false;
}


AEDIT( aedit_view_trade )
{
    char buf[MAX_STRING_LENGTH];
    AREA_DATA *pTArea;
    TRADE_ITEM *pItem;
    int i = 0;

    char arg[MAX_STRING_LENGTH];

    argument = one_argument( argument, arg );

    if ( ( arg[0] == '\0' ) || (( i = get_trade_item( arg )) == 0 ) )
    {
        send_to_char("viewtrade 'trade type'\n\r\n\rAvailable trade types are:\n", ch);

        while( trade_table[ i ].trade_type != TRADE_LAST )
        {
            send_to_char( trade_table[ i ].name, ch );
            send_to_char( "\n\r", ch );
            i++;
        }

        return false;
    }

    sprintf( buf, "Showing all trade types across areas for {G%s{x:\n\r",
            trade_table[i].name );
    send_to_char( buf, ch );
    send_to_char( "\n\rArea            Type        Rep. Amt.   Rep Time.   Qty    MaxQty     Min    Max    Buy    Sell\n\r", ch );
    send_to_char( "----------------------------------------------------------------------------------------------------\n\r", ch );

    for ( pTArea = area_first; pTArea != NULL; pTArea = pTArea->next )
    {
        for ( pItem = pTArea->trade_list; pItem != NULL; pItem = pItem->next )
        {
            if ( pItem->trade_type == i )
            {
                sprintf( buf, "%-16s %-15s %-12ld %-9ld %-7ld %-7ld %-7ld %-7ld %-7ld %-7ld\n\r",
                        pTArea->name, ( pItem->replenish_amount > 0 ) ? "{RSupplier{x" : "{GConsumer{x",
                        pItem->replenish_amount,
                        pItem->replenish_time,
                        pItem->qty,
                        pItem->max_qty,
                        pItem->min_price,
                        pItem->max_price,
                        pItem->buy_price,
                        pItem->sell_price );
                send_to_char( buf, ch );
            }
        }
    }

    return true;
}


AEDIT( aedit_remove_trade)
{
    AREA_DATA *pArea;
    char arg1[MAX_STRING_LENGTH];
    TRADE_ITEM *type;
    TRADE_ITEM *temp;

    EDIT_AREA(ch, pArea);

    argument = one_argument( argument, arg1);
    //sprintf(buf, "%s %s %s %s\n\r", arg1, arg2, arg3, arg4);
    //send_to_char(buf, ch);
    if ( arg1[0] == '\0')// || arg2[0] == '\0' || arg3[0] == '\0' || arg4[0] == '\0')
    {
    send_to_char("removetrade name\n\r", ch);
    return false;
    }

    type = find_trade_item(pArea, arg1);

    if (type == NULL)
    {
    send_to_char("That is not a valid trade item.\n\r", ch);
    return false;
    }


    if  ( type == pArea->trade_list )
    {
    pArea->trade_list = type->next;
    }
    else
    for ( temp = pArea->trade_list; temp; temp = temp->next )
    {
        if ( temp->next == type )
        {
        temp->next = type->next;
        break;
        }
    }

    free_trade_item(type);
    send_to_char("Trade item removed.\n\r", ch);

    return false;
}

AEDIT(aedit_open)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_bool(ch, argument, "Open", NULL, &pArea->open,
        NULL, NULL);
}


AEDIT(aedit_create)
{
    AREA_DATA *pArea;
    char filename[MSL];

    pArea               =   new_area();
    pArea->uid = gconfig.next_area_uid++;
    gconfig_write();

    snprintf(filename, sizeof(filename), "area%ld.json", pArea->uid);
    free_string(pArea->file_name);
    pArea->file_name = str_dup(filename);

    area_last->next     =   pArea;
    area_last           =   pArea;      /* Thanks, Walker. */
    ch->desc->pEdit     =   (void *)pArea;

    SET_BIT(pArea->area_flags, AREA_ADDED);
    send_to_char("Area Created.\n\r", ch);
    return true;
}


AEDIT(aedit_regions)
{
    AREA_DATA *pArea;
    char arg1[MIL];
    char arg2[MIL];
    char buf[MSL];

    EDIT_AREA(ch, pArea);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (IS_NULLSTR(arg1) || !str_prefix(arg1, "list"))
    {
        BUFFER *output = new_buf();
        int idx = 0;
        bool append_ok = true;
        AREA_REGION *region;
        ITERATOR it;

        sprintf(buf, "Default: who={W%s{x place={W%s{x flags={W%s{x rooms={W%d{x\n\r",
            flag_string(area_who_titles, pArea->region.area_who),
            flag_string(place_flags, pArea->region.rs_place_flags),
            flag_string(area_region_flags, pArea->region.flags),
            pArea->region.rooms ? (int)list_size(pArea->region.rooms) : 0);
        append_ok = add_buf(output, buf);
        sprintf(buf, "         topic={W%s{x\n\r", IS_NULLSTR(pArea->region.topic) ? "(default)" : pArea->region.topic);
        append_ok = append_ok && add_buf(output, buf);
        sprintf(buf, "         weather: density={W%d%%{x life={W%d%%{x severity={W%d{x\n\r",
            pArea->region.weather_density_percent,
            pArea->region.weather_life_percent,
            pArea->region.weather_severity_bias);
        append_ok = append_ok && add_buf(output, buf);
        append_ok = append_ok && add_buf(output, "\n\r");
        append_ok = append_ok && add_buf(output, " #   UID   Name                     Topic                Who              Place             Flags           Rooms Weather\n\r");
        append_ok = append_ok && add_buf(output, "----------------------------------------------------------------------------------------------------------------------\n\r");

        if (!append_ok)
        {
            send_to_char("Region output exceeded buffer limits.\n\r", ch);
            free_buf(output);
            return false;
        }

        iterator_start(&it, pArea->regions);
        while ((region = (AREA_REGION *)iterator_nextdata(&it)))
        {
            sprintf(buf, "%2d  %4ld  %-24.24s %-20.20s %-16.16s %-16.16s %-14.14s %5d %3d/%3d/%+4d\n\r",
                ++idx,
                region->uid,
                region->name ? region->name : "",
                IS_NULLSTR(region->topic) ? "(default)" : region->topic,
                flag_string(area_who_titles, region->area_who),
                flag_string(place_flags, region->rs_place_flags),
                flag_string(area_region_flags, region->flags),
                region->rooms ? (int)list_size(region->rooms) : 0,
                region->weather_density_percent,
                region->weather_life_percent,
                region->weather_severity_bias);
            if (!add_buf(output, buf))
            {
                append_ok = false;
                break;
            }
        }
        iterator_stop(&it);

        if (!append_ok)
        {
            send_to_char("Region output exceeded buffer limits.\n\r", ch);
            free_buf(output);
            return false;
        }

        if (idx == 0)
        {
            if (!add_buf(output, "(No custom regions)\n\r"))
            {
                send_to_char("Region output exceeded buffer limits.\n\r", ch);
                free_buf(output);
                return false;
            }
        }

        page_to_char(buf_string(output), ch);
        free_buf(output);
        return false;
    }

    if (!str_prefix(arg1, "add"))
    {
        AREA_REGION *region;

        if (IS_NULLSTR(arg2) && IS_NULLSTR(argument))
        {
            send_to_char("Syntax: regions add <name>\n\r", ch);
            return false;
        }

        region = new_area_region();
        region->area = pArea;
        region->uid = ++pArea->top_region_uid;
        free_string(region->name);
        region->name = str_dup(IS_NULLSTR(arg2) ? argument : formatf("%s %s", arg2, argument));
        region->area_who = pArea->region.area_who;
        region->rs_place_flags = pArea->region.rs_place_flags;
        region->rs_x = pArea->region.rs_x;
        region->rs_y = pArea->region.rs_y;
        region->rs_land_x = pArea->region.rs_land_x;
        region->rs_land_y = pArea->region.rs_land_y;
        region->weather_density_percent = pArea->region.weather_density_percent;
        region->weather_life_percent = pArea->region.weather_life_percent;
        region->weather_severity_bias = pArea->region.weather_severity_bias;

        list_appendlink(pArea->regions, region);
        send_to_char("Region added.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg1, "remove"))
    {
        AREA_REGION *remove_me;
        ROOM_INDEX_DATA *room;
        int region_no;

        if (list_size(pArea->regions) < 1)
        {
            send_to_char("There are no regions to remove.\n\r", ch);
            return false;
        }

        if (!is_number(arg2))
        {
            send_to_char("Syntax: regions remove <#>\n\r", ch);
            return false;
        }

        region_no = atoi(arg2);
        if (region_no < 1 || region_no > list_size(pArea->regions))
        {
            sprintf(buf, "Please specify a number from 1 to %d.\n\r", (int)list_size(pArea->regions));
            send_to_char(buf, ch);
            return false;
        }

        remove_me = (AREA_REGION *)list_nthdata(pArea->regions, region_no);
        while ((room = (ROOM_INDEX_DATA *)list_nthdata(remove_me->rooms, 1)) != NULL)
            area_region_add_room(&pArea->region, room);

        list_remnthlink(pArea->regions, region_no, true);
        send_to_char("Region removed.\n\r", ch);
        return true;
    }

    AREA_REGION *region = NULL;
    if (!str_prefix(arg2, "default"))
        region = &pArea->region;
    else if (is_number(arg2))
    {
        int region_no = atoi(arg2);
        if (region_no >= 1 && region_no <= list_size(pArea->regions))
            region = (AREA_REGION *)list_nthdata(pArea->regions, region_no);
    }
    else if (!IS_NULLSTR(arg2))
    {
        AREA_REGION *r;
        ITERATOR it;
        iterator_start(&it, pArea->regions);
        while ((r = (AREA_REGION *)iterator_nextdata(&it)))
        {
            if (r->name && !str_prefix(arg2, r->name))
            {
                region = r;
                break;
            }
        }
        iterator_stop(&it);
    }

    if (!region)
    {
        send_to_char("Please specify <#|name|default>.\n\r", ch);
        return false;
    }

    if (!str_prefix(arg1, "name"))
    {
        if (IS_NULLSTR(argument))
        {
            send_to_char("Syntax: regions name <#|name|default> <name>\n\r", ch);
            return false;
        }

        free_string(region->name);
        region->name = str_dup(argument);
        send_to_char("Region name changed.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg1, "topic"))
    {
        if (IS_NULLSTR(argument))
        {
            send_to_char("Syntax: regions topic <#|name|default> <topic|clear>\n\r", ch);
            return false;
        }

        free_string(region->topic);
        if (!str_prefix(argument, "clear"))
            region->topic = str_dup("");
        else
            region->topic = str_dup(argument);

        send_to_char("Region topic changed.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg1, "description"))
    {
        string_append(ch, &region->description);
        return true;
    }

    if (!str_prefix(arg1, "comments"))
    {
        string_append(ch, &region->comments);
        return true;
    }

    if (!str_prefix(arg1, "flags"))
    {
        long value;
        if ((value = flag_value(area_region_flags, argument)) == NO_FLAG)
        {
            send_to_char("Syntax: regions flags <#|name|default> <flag>\n\r", ch);
            send_to_char("Type '? area_region_flags' for values.\n\r", ch);
            return false;
        }

        TOGGLE_BIT(region->flags, value);
        send_to_char("Region flags toggled.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg1, "who"))
    {
        int value;
        if (!str_prefix(argument, "blank"))
            value = AREA_BLANK;
        else if ((value = flag_value(area_who_titles, argument)) == NO_FLAG)
        {
            send_to_char("Syntax: regions who <#|name|default> <title|blank>\n\r", ch);
            send_to_char("Type '? areawho' for values.\n\r", ch);
            return false;
        }

        region->area_who = value;
        send_to_char("Region who title set.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg1, "place"))
    {
        int value;
        if (!str_cmp(argument, "none"))
            value = PLACE_NOWHERE;
        else if ((value = flag_value(place_flags, argument)) == NO_FLAG)
        {
            send_to_char("Syntax: regions place <#|name|default> <placetype|none>\n\r", ch);
            send_to_char("Type '? placetype' for values.\n\r", ch);
            return false;
        }

        region->rs_place_flags = value;
        send_to_char("Region place type set.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg1, "weather"))
    {
        char arg3[MIL];
        char arg4[MIL];
        int value;

        argument = one_argument(argument, arg3);
        argument = one_argument(argument, arg4);

        if (IS_NULLSTR(arg3) || !str_prefix(arg3, "show"))
        {
            sprintf(buf,
                "Region weather for '%s': density=%d%% life=%d%% severity=%d\n\r",
                IS_NULLSTR(region->name) ? "default" : region->name,
                region->weather_density_percent,
                region->weather_life_percent,
                region->weather_severity_bias);
            send_to_char(buf, ch);
            return false;
        }

        if (!str_prefix(arg3, "density"))
        {
            if (!is_number(arg4))
            {
                send_to_char("Syntax: regions weather <#|name|default> density <25-300>\n\r", ch);
                return false;
            }

            value = atoi(arg4);
            region->weather_density_percent = URANGE(25, value, 300);
            send_to_char("Region weather density set.\n\r", ch);
            return true;
        }

        if (!str_prefix(arg3, "life"))
        {
            if (!is_number(arg4))
            {
                send_to_char("Syntax: regions weather <#|name|default> life <25-300>\n\r", ch);
                return false;
            }

            value = atoi(arg4);
            region->weather_life_percent = URANGE(25, value, 300);
            send_to_char("Region weather life scale set.\n\r", ch);
            return true;
        }

        if (!str_prefix(arg3, "severity"))
        {
            if (!is_number(arg4))
            {
                send_to_char("Syntax: regions weather <#|name|default> severity <-100..100>\n\r", ch);
                return false;
            }

            value = atoi(arg4);
            region->weather_severity_bias = URANGE(-100, value, 100);
            send_to_char("Region weather severity bias set.\n\r", ch);
            return true;
        }

        send_to_char("Syntax: regions weather <#|name|default> show\n\r", ch);
        send_to_char("        regions weather <#|name|default> density <25-300>\n\r", ch);
        send_to_char("        regions weather <#|name|default> life <25-300>\n\r", ch);
        send_to_char("        regions weather <#|name|default> severity <-100..100>\n\r", ch);
        return false;
    }

    send_to_char("Syntax: regions list\n\r", ch);
    send_to_char("        regions add <name>\n\r", ch);
    send_to_char("        regions remove <#>\n\r", ch);
    send_to_char("        regions name <#|name|default> <name>\n\r", ch);
    send_to_char("        regions topic <#|name|default> <topic|clear>\n\r", ch);
    send_to_char("        regions description <#|name|default>\n\r", ch);
    send_to_char("        regions comments <#|name|default>\n\r", ch);
    send_to_char("        regions flags <#|name|default> <flag>\n\r", ch);
    send_to_char("        regions who <#|name|default> <title|blank>\n\r", ch);
    send_to_char("        regions place <#|name|default> <placetype|none>\n\r", ch);
    send_to_char("        regions weather <#|name|default> <show|density|life|severity> [value]\n\r", ch);
    return false;
}


AEDIT(aedit_topic)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);

    return olc_cmd_string(ch, argument, "Topic", NULL,
        &pArea->area_topic, OLC_STR_CLEARABLE, NULL, NULL);
}


AEDIT(aedit_name)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    bool changed = olc_cmd_string(ch, argument, "Name", NULL, &pArea->name,
        OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, NULL, NULL);
    if (changed)
        aedit_rebuild_auto_tags(pArea);
    return changed;
}

AEDIT(aedit_tags)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_string(ch, argument, "Tags", NULL, &pArea->tags,
        OLC_STR_CLEARABLE, NULL, NULL);
}

AEDIT(aedit_desc)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &pArea->description, NULL, NULL);
}

AEDIT(aedit_comments)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &pArea->comments, NULL, NULL);
}

AEDIT(aedit_notes)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_string_append(ch, argument, "Notes", NULL,
        &pArea->notes, NULL, NULL);
}


AEDIT(aedit_repop)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_number(ch, argument, "Repop Time",
        "Syntax:  repop [5-120 minutes]\n\r",
        &pArea->repop, 5, 120, NULL, NULL);
}


AEDIT(aedit_credits)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    return olc_cmd_string(ch, argument, "Credits", NULL, &pArea->credits,
        OLC_STR_DEFAULT, NULL, NULL);
}


AEDIT(aedit_areawho)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  areawho [title]\n\r"
              "Type '? areawho' for a list of who titles.\n\r", ch);
        return false;
    }

    /* "blank" is a special alias for AREA_BLANK (0) */
    if (!str_prefix(argument, "blank")) {
        char zero_str[] = "0";
        return olc_cmd_type_set(ch, zero_str, "Area Who", NULL,
            &pArea->area_who, area_who_titles, NULL, NULL);
    }

    int value = flag_value(area_who_titles, argument);
    if (value == NO_FLAG) {
        send_to_char("Syntax:  areawho [title]\n\r"
              "Type '? areawho' for a list of who titles.\n\r", ch);
        return false;
    }

    if (value == AREA_INSTANCE || value == AREA_DUTY) {
        send_to_char("Area who title only allowed in blueprints.\n\r", ch);
        return false;
    }

    return olc_cmd_type_set(ch, argument, "Area Who", NULL,
        &pArea->area_who, area_who_titles, NULL, NULL);
}

AEDIT(aedit_placetype)
{
    AREA_DATA *pArea;

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  placetype [flag]\n\r"
              "Type '? placetype' for a list of possible values.\n\r", ch);
        return false;
    }

    EDIT_AREA(ch, pArea);

    long new_value;
    if (!str_cmp(argument, "none")) {
        new_value = PLACE_NOWHERE;
    } else {
        long val = flag_value(place_flags, argument);
        if (val == NO_FLAG) {
            send_to_char("Syntax:  placetype [flag]\n\r"
                  "Type '? placetype' for a list of possible values.\n\r", ch);
            return false;
        }
        new_value = val;
    }

    /* Staged mode */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            if (!olc_check_staging_limits(ch, cs)) return false;
            json_t *old_val = json_integer(pArea->place_flags);
            json_t *new_val_j = json_integer(new_value);
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, "Place Type", OLC_FIELD_LONG, old_val, new_val_j);
            json_decref(old_val);
            json_decref(new_val_j);
            if (result)
                send_to_char("{G[STAGED]{x Place type staged.\n\r", ch);
            else
                send_to_char("Place type reverted to original value.\n\r", ch);
            return result != NULL;
        }
    }

    /* Non-staged fallback */
    pArea->place_flags = new_value;
    send_to_char("Area place type set.\n\r", ch);
    return true;
}


AEDIT(aedit_file)
{
    AREA_DATA *pArea;
    char file[MAX_STRING_LENGTH];
    int i, length;

    EDIT_AREA(ch, pArea);

    one_argument(argument, file);	/* Forces Lowercase */

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  filename [$file]\n\r", ch);
        return false;
    }

    length = strlen(file);
    if (length > 12)
    {
        send_to_char("No more than twelve characters allowed.\n\r", ch);
        return false;
    }

    for (i = 0; i < length; i++)
    {
        if (!ISALNUM(file[i]))
        {
            send_to_char("Only letters and numbers are valid.\n\r", ch);
            return false;
        }
    }

    strcat(file, ".json");
    return olc_cmd_string(ch, file, "File Name", NULL,
        &pArea->file_name, 0, NULL, NULL);
}


AEDIT(aedit_age)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);

    return olc_cmd_number_i16(ch, argument, "Age",
        "Syntax:  age [#xage]\n\r",
        &pArea->age, 0, INT16_MAX, NULL, NULL);
}


AEDIT(aedit_recall)
{
    AREA_DATA *pArea;
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MIL];
    char arg4[MIL];

    EDIT_AREA(ch, pArea);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);

    if (!arg1[0]) {
        send_to_char("Syntax:  recall <widevnum>\n\r", ch);
        send_to_char("         recall <wuid> <x> <y> <z>\n\r", ch);
        send_to_char("         recall 0 (to clear)\n\r", ch);
        return false;
    }

    /* Determine what we're staging */
    char stage_val[MIL];

    if (!str_cmp(arg1, "0") && !arg2[0]) {
        strlcpy(stage_val, "0", sizeof(stage_val));
    } else if (!arg2[0]) {
        /* Single arg - widevnum (room format) */
        WNUM room_wnum;
        AREA_DATA *context = olc_relative_widevnum_context(pArea, arg1);
        if (!parse_widevnum(arg1, context, &room_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        if (!get_room_index(room_wnum.pArea, room_wnum.vnum)) {
            send_to_char("AEdit:  Room vnum does not exist.\n\r", ch);
            return false;
        }
        snprintf(stage_val, sizeof(stage_val), "room %ld", room_wnum.vnum);
    } else {
        /* Multi-arg - wilderness format */
        if (!arg3[0] || !arg4[0] || !is_number(arg1) || !is_number(arg2) || !is_number(arg3) || !is_number(arg4)) {
            send_to_char("Syntax:  recall <widevnum>\n\r", ch);
            send_to_char("         recall <wuid> <x> <y> <z>\n\r", ch);
            return false;
        }
        long wuid = atol(arg1);
        if (!get_wilds_from_uid(NULL, wuid)) {
            send_to_char("AEdit:  Wilderness UID does not exist.\n\r", ch);
            return false;
        }
        snprintf(stage_val, sizeof(stage_val), "wilds %s %s %s %s", arg1, arg2, arg3, arg4);
    }

    /* Staged mode */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            if (!olc_check_staging_limits(ch, cs)) return false;

            /* Build old value string */
            char old_buf[MIL];
            if (pArea->recall.wuid)
                snprintf(old_buf, sizeof(old_buf), "wilds %lu %lu %lu %lu",
                    pArea->recall.wuid, pArea->recall.id[0], pArea->recall.id[1], pArea->recall.id[2]);
            else if (pArea->recall.id[0] > 0)
                snprintf(old_buf, sizeof(old_buf), "room %lu", pArea->recall.id[0]);
            else
                strlcpy(old_buf, "0", sizeof(old_buf));

            json_t *old_val = json_string(old_buf);
            json_t *new_val = json_string(stage_val);
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, "Recall", OLC_FIELD_STRING, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);
            if (result)
                send_to_char("{G[STAGED]{x Recall staged.\n\r", ch);
            else
                send_to_char("Recall reverted to original value.\n\r", ch);
            return result != NULL;
        }
    }

    /* Non-staged fallback */
    if (!str_cmp(arg1, "0") && !arg2[0]) {
        location_clear(&pArea->recall);
        send_to_char("Recall cleared.\n\r", ch);
        return true;
    }

    if (!arg2[0]) {
        WNUM room_wnum;
        AREA_DATA *context = olc_relative_widevnum_context(pArea, arg1);
        parse_widevnum(arg1, context, &room_wnum);
        location_set(&pArea->recall, 0, room_wnum.vnum, 0, 0);
        send_to_char("Recall set.\n\r", ch);
        return true;
    }

    long wuid = atol(arg1);
    int x = atoi(arg2);
    int y = atoi(arg3);
    int z = atoi(arg4);
    location_set(&pArea->recall, wuid, x, y, z);
    send_to_char("Recall set.\n\r", ch);
    return true;
}


AEDIT(aedit_security)
{
    AREA_DATA *pArea;
    char sec[MAX_STRING_LENGTH];
    int value;

    EDIT_AREA(ch, pArea);

    one_argument(argument, sec);

    if (!is_number(sec) || sec[0] == '\0') {
        send_to_char("Syntax:  security [#xlevel]\n\r", ch);
        return false;
    }

    value = atoi(sec);

    if (value > ch->pcdata->security || value < 0) {
        if (ch->pcdata->security != 0) {
            char buf[MAX_STRING_LENGTH];
            sprintf(buf, "Security is 0-%d.\n\r", ch->pcdata->security);
            send_to_char(buf, ch);
        } else {
            send_to_char("Security is 0 only.\n\r", ch);
        }
        return false;
    }

    return olc_cmd_number(ch, sec, "Security", NULL,
        &pArea->security, 0, ch->pcdata->security, NULL, NULL);
}


AEDIT(aedit_builder)
{
    AREA_DATA *pArea;
    char name[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];

    EDIT_AREA(ch, pArea);

    one_argument(argument, name);

    if (name[0] == '\0')
    {
        send_to_char("Syntax:  builder [$name]  -toggles builder\n\r", ch);
        send_to_char("Syntax:  builder All      -allows everyone\n\r", ch);
        return false;
    }

    name[0] = UPPER(name[0]);

    if (strstr(pArea->builders, name) != NULL)
    {
        pArea->builders = string_replace(pArea->builders, name, "\0");
        pArea->builders = string_unpad(pArea->builders);

        if (pArea->builders[0] == '\0')
        {
            free_string(pArea->builders);
            pArea->builders = str_dup("None");
        }
        send_to_char("Builder removed.\n\r", ch);
        return true;
    }
    else
    {
        buf[0] = '\0';

        if (!player_exists(name) && str_cmp(name, "All"))
        {
            act("There is no character by the name of $t.", ch, NULL, NULL, NULL, NULL, name, NULL, TO_CHAR, NULL, NULL);
            return false;
        }

        if (strstr(pArea->builders, "None") != NULL)
        {
            pArea->builders = string_replace(pArea->builders, "None", "\0");
            pArea->builders = string_unpad(pArea->builders);
        }

        if (pArea->builders[0] != '\0')
        {
            strcat(buf, pArea->builders);
            strcat(buf, " ");
        }

        strcat(buf, name);
        free_string(pArea->builders);
        pArea->builders = string_proper(str_dup(buf));

        send_to_char("Builder added.\n\r", ch);
        send_to_char(pArea->builders,ch);
        return true;
    }

    return false;
}


AEDIT(aedit_vnum)
{
    AREA_DATA *pArea;
    char lower[MAX_STRING_LENGTH];
    char upper[MAX_STRING_LENGTH];
    long ilower;
    long iupper;

    EDIT_AREA(ch, pArea);

    argument = one_argument(argument, lower);
    one_argument(argument, upper);

    if (!is_number(lower) || lower[0] == '\0'
    || !is_number(upper) || upper[0] == '\0')
    {
        send_to_char("Syntax:  vnum [#xlower] [#xupper]\n\r", ch);
        return false;
    }

    ilower = atol(lower);
    iupper = atol(upper);

    if (ilower > iupper)
    {
        send_to_char("AEdit:  Upper must be larger then lower.\n\r", ch);
        return false;
    }

    if (!check_range(ilower, iupper))
    {
        send_to_char("AEdit:  Range must include only this area.\n\r", ch);
        return false;
    }

    if (get_vnum_area(ilower)
    && get_vnum_area(ilower) != pArea)
    {
        send_to_char("AEdit:  Lower vnum already assigned.\n\r", ch);
        return false;
    }

    if (get_vnum_area(iupper)
    && get_vnum_area(iupper) != pArea)
    {
        send_to_char("AEdit:  Upper vnum already assigned.\n\r", ch);
        return false;
    }

    /* Staged mode */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            if (!olc_check_staging_limits(ch, cs)) return false;

            json_t *old_lo = json_integer(pArea->min_vnum);
            json_t *new_lo = json_integer(ilower);
            olc_changeset_add_change(cs, "Min Vnum", OLC_FIELD_LONG, old_lo, new_lo);
            json_decref(old_lo);
            json_decref(new_lo);

            json_t *old_hi = json_integer(pArea->max_vnum);
            json_t *new_hi = json_integer(iupper);
            olc_changeset_add_change(cs, "Max Vnum", OLC_FIELD_LONG, old_hi, new_hi);
            json_decref(old_hi);
            json_decref(new_hi);

            send_to_char("{G[STAGED]{x Vnum range staged.\n\r", ch);
            return true;
        }
    }

    /* Non-staged fallback */
    pArea->min_vnum = ilower;
    send_to_char("Lower vnum set.\n\r", ch);
    pArea->max_vnum = iupper;
    send_to_char("Upper vnum set.\n\r", ch);
    return true;
}

AEDIT(aedit_levels)
{
    AREA_DATA *pArea;
    char lower[MAX_STRING_LENGTH];
    char upper[MAX_STRING_LENGTH];
    int ilower;
    int iupper;

    EDIT_AREA(ch, pArea);

    argument = one_argument(argument, lower);
    one_argument(argument, upper);

    if (!is_number(lower) || lower[0] == '\0'
    || !is_number(upper) || upper[0] == '\0')
    {
        send_to_char("Syntax:  levels [#xlower] [#xupper]\n\r", ch);
        return false;
    }

    ilower = atoi(lower);
    iupper = atoi(upper);

    if (ilower > iupper)
    {
        send_to_char("AEdit:  Upper must be larger then lower.\n\r", ch);
        return false;
    }

    if (ilower > 120 || iupper < 1)
    {
        send_to_char("AEdit:  Range must be between 1 and 120.\n\r", ch);
        return false;
    }

    /* Staged mode */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            if (!olc_check_staging_limits(ch, cs)) return false;

            json_t *old_lo = json_integer(pArea->min_level);
            json_t *new_lo = json_integer(ilower);
            olc_changeset_add_change(cs, "Min Level", OLC_FIELD_INT16, old_lo, new_lo);
            json_decref(old_lo);
            json_decref(new_lo);

            json_t *old_hi = json_integer(pArea->max_level);
            json_t *new_hi = json_integer(iupper);
            olc_changeset_add_change(cs, "Max Level", OLC_FIELD_INT16, old_hi, new_hi);
            json_decref(old_hi);
            json_decref(new_hi);

            send_to_char("{G[STAGED]{x Level range staged.\n\r", ch);
            return true;
        }
    }

    /* Non-staged fallback */
    pArea->min_level = ilower;
    send_to_char("Lower level set.\n\r", ch);
    pArea->max_level = iupper;
    send_to_char("Upper level set.\n\r", ch);
    return true;
}


AEDIT(aedit_postoffice)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0') {
        send_to_char("Syntax:   postoffice <widevnum in the area>\n\r", ch);
        return false;
    }

    const char *stage_val = argument;
    WNUM room_wnum;
    ROOM_INDEX_DATA *room = NULL;

    if (str_cmp(argument, "0") && str_cmp(argument, "none") && str_cmp(argument, "clear")) {
        if (!parse_widevnum(argument, pArea, &room_wnum) || room_wnum.pArea != pArea) {
            send_to_char("That room must be in the current area.\n\r", ch);
            return false;
        }
        room = get_room_index(room_wnum.pArea, room_wnum.vnum);
        if (!room) {
            send_to_char("That room vnum doesn't exist.\n\r", ch);
            return false;
        }
    }

    /* Staged mode */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            if (!olc_check_staging_limits(ch, cs)) return false;

            char old_buf[MAX_INPUT_LENGTH];
            if (pArea->post_office_load.vnum > 0)
                snprintf(old_buf, sizeof(old_buf), "%ld#%ld",
                    pArea->post_office_load.auid, pArea->post_office_load.vnum);
            else
                strlcpy(old_buf, "none", sizeof(old_buf));

            json_t *old_val = json_string(old_buf);
            json_t *new_val = json_string(stage_val);
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, "Post Office", OLC_FIELD_STRING, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);
            if (result)
                send_to_char("{G[STAGED]{x Post office staged.\n\r", ch);
            else
                send_to_char("Post office reverted to original value.\n\r", ch);
            return result != NULL;
        }
    }

    /* Non-staged fallback */
    if (!str_cmp(argument, "0") || !str_cmp(argument, "none") || !str_cmp(argument, "clear")) {
        pArea->post_office_load.auid = 0;
        pArea->post_office_load.vnum = 0;
        pArea->post_office_wnum.pArea = NULL;
        pArea->post_office_wnum.vnum = 0;
        send_to_char("Post office cleared.\n\r", ch);
        return true;
    }

    pArea->post_office_load.auid = pArea->uid;
    pArea->post_office_load.vnum = room_wnum.vnum;
    pArea->post_office_wnum = room_wnum;
    printf_to_char(ch, "Set post office of %s to %s(%s)\n\r",
        pArea->name, room->name, widevnum_string_room(room, pArea));
    return true;
}

AEDIT (aedit_addaprog)
{
    int tindex, slot;
    AREA_DATA *pArea;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_AREA(ch, pArea);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (num[0] == '\0' || trigger[0] =='\0' || phrase[0] =='\0')
    {
        send_to_char("Syntax:   addaprog [widevnum] [trigger] [phrase]\n\r",ch);
        return false;
    }

    if ((tindex = trigger_index(trigger, PRG_APROG)) < 0) {
        send_to_char("Valid flags are:\n\r",ch);
        show_help(ch, "aprog");
        return false;
    }

    slot = trigger_table[tindex].slot;

    WNUM script_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(pArea, num);
    if (!parse_widevnum(num, context, &script_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if ((code = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_APROG)) == NULL)
    {
        send_to_char("No such AREAProgram.\n\r",ch);
        return false;
    }

    // Make sure this has a list of progs!
    if(!pArea->progs->progs) pArea->progs->progs = new_prog_bank();

    if (edit_trigger_exists(pArea->progs->progs, code, tindex, phrase)) {
        send_to_char("That trigger/phrase pair is already attached to that script on this area.\n\r", ch);
        return false;
    }

    list                  = new_trigger();
    list->vnum            = script_wnum.vnum;
    list->script_is_widevnum = (script_wnum.pArea != NULL);
    if (list->script_is_widevnum) { list->script_load.auid = script_wnum.pArea->uid; list->script_load.vnum = script_wnum.vnum; }
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
    if (is_widevnum_format(phrase)) {
        list->numeric = true;
        list->trig_is_widevnum = true;
        parse_widevnum_load(phrase, &list->trig_load);
        list->trig_number = (int)list->trig_load.vnum;
    } else {
        list->trig_number = atoi(list->trig_phrase);
        list->numeric = is_number(list->trig_phrase);
    }

    list->script          = code;
    //SET_BIT(pMob->mprog_flags,value);

    list_appendlink(pArea->progs->progs[slot], list);

    send_to_char("Aprog Added.\n\r",ch);
    return true;
}


AEDIT (aedit_delaprog)
{
    AREA_DATA *pArea;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int group_idx, trig_idx;
    PROG_GROUP groups[MAX_PROG_GROUPS];
    int num_groups;

    EDIT_AREA(ch, pArea);

    if (!pArea->progs->progs) {
        send_to_char("This area has no programs attached.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  delaprog <group#>\n\r", ch);
        send_to_char("         delaprog <group#> <trigger#>\n\r", ch);
        return false;
    }

    if (!is_number(arg1)) {
        send_to_char("Please specify a valid group number.\n\r", ch);
        return false;
    }

    group_idx = atoi(arg1);
    num_groups = prog_build_groups(pArea->progs->progs, groups, MAX_PROG_GROUPS, PRG_APROG);

    if (group_idx < 1 || group_idx > num_groups) {
        send_to_char("Invalid group number.\n\r", ch);
        return false;
    }

    PROG_GROUP *group = &groups[group_idx - 1];

    if (arg2[0] == '\0') {
        // Delete entire group (all triggers for this script)
        if (edit_delscript(pArea->progs->progs, group->script)) {
            send_to_char("Script group removed.\n\r", ch);
            return true;
        }
    } else {
        // Delete specific trigger within group
        if (!is_number(arg2)) {
            send_to_char("Please specify a valid trigger number within the group.\n\r", ch);
            return false;
        }

        trig_idx = atoi(arg2);
        if (trig_idx < 1 || trig_idx > group->trigger_count) {
            send_to_char("Invalid trigger number within that group.\n\r", ch);
            return false;
        }

        PROG_GROUP_ENTRY *entry = &group->triggers[trig_idx - 1];
        if (edit_deltrigger_specific(pArea->progs->progs, group->script, entry->entry->trig_type, entry->entry->trig_phrase)) {
            send_to_char("Trigger removed from script group.\n\r", ch);
            return true;
        }
    }

    send_to_char("No such program or trigger found.\n\r", ch);
    return false;
}

AEDIT(aedit_varset)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            char varname[MAX_INPUT_LENGTH - 8];
            one_argument(argument, varname);
            if (IS_NULLSTR(varname)) {
                send_to_char("Syntax: varset <name> <type> <value>\n\r", ch);
                return false;
            }
            char field_path[MAX_INPUT_LENGTH];
            snprintf(field_path, sizeof(field_path), "var/%s", varname);

            if (!olc_check_staging_limits(ch, cs)) return false;
            json_t *old_val = json_null();
            json_t *new_val = json_string(argument);
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, field_path, OLC_FIELD_STRING, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);

            if (result)
                printf_to_char(ch, "{G[STAGED]{x Variable %s staged.\n\r", varname);
            else
                printf_to_char(ch, "Variable %s reverted.\n\r", varname);
            return result != NULL;
        }
    }

    if (olc_varset(&pArea->index_vars, ch, argument, false))
    {
        olc_varset(&pArea->progs->vars, ch, argument, true);
        return true;
    }
    return false;
}

AEDIT(aedit_varclear)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            char varname[MAX_INPUT_LENGTH - 8];
            one_argument(argument, varname);
            if (IS_NULLSTR(varname)) {
                send_to_char("Syntax: varclear <name>\n\r", ch);
                return false;
            }
            char field_path[MAX_INPUT_LENGTH];
            snprintf(field_path, sizeof(field_path), "var/%s", varname);

            if (!olc_check_staging_limits(ch, cs)) return false;
            json_t *old_val = json_null();
            json_t *new_val = json_null();
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, field_path, OLC_FIELD_STRING, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);

            if (result)
                printf_to_char(ch, "{G[STAGED]{x Variable %s clear staged.\n\r", varname);
            else
                printf_to_char(ch, "Variable %s reverted.\n\r", varname);
            return result != NULL;
        }
    }

    if (olc_varclear(&pArea->index_vars, ch, argument, false))
    {
        olc_varclear(&pArea->progs->vars, ch, argument, true);
        return true;
    }

    return false;
}
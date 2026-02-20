/***************************************************************************
 *  File: olc_act.c                                                        *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 *                                                                         *
 *  This code was freely distributed with the The Isles 1.1 source code,   *
 *  and has been used here for OLC - OLC would not be what it is without   *
 *  all the previous coders who released their source code.                *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../../item_types.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

extern void oedit_show_type_data(OBJ_INDEX_DATA *pObj, BUFFER *buffer);
bool set_obj_values(CHAR_DATA *ch, OBJ_INDEX_DATA *pObj, int value_num, char *argument);

/* Forward declarations for tab show functions */
static void oedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void oedit_show_properties_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void oedit_show_affects_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void oedit_show_scripts_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void oedit_show_type_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

static AREA_DATA *oedit_get_area(void *pEdit)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)pEdit;
    return pObj ? pObj->area : NULL;
}

/*
 * Object Editor Command Table
 */
const struct olc_cmd_type oedit_table[] =
{
    { "?",              show_help           },
    { "addaffect",      oedit_addaffect     },
    { "addcatalyst",    oedit_addcatalyst   },
    { "addimmune",      oedit_addimmune     },
    { "addoprog",       oedit_addoprog      },
    { "addskill",       oedit_addskill      },
    { "addspell",       oedit_addspell      },
    { "addtype",        oedit_addtype       },
    { "allowedfixed",   oedit_allowed_fixed },
    { "commands",       show_commands       },
    { "comments",       oedit_comments      },
    { "condition",      oedit_condition     },
    { "cost",           oedit_cost          },
    { "create",         oedit_create        },
    { "delaffect",      oedit_delaffect     },
    { "delcatalyst",    oedit_delcatalyst   },
    { "delimmune",      oedit_delimmune     },
    { "deloprog",       oedit_deloprog      },
    { "delspell",       oedit_delspell      },
    { "description",    oedit_desc          },
    { "ed",             oedit_ed            },
    { "extra",          oedit_extra         },
    { "fragility",      oedit_fragility     },
    { "level",          oedit_level         },
    { "lock",           oedit_lock          },
    { "long",           oedit_long          },
    { "material",       oedit_material      },
    { "name",           oedit_name          },
    { "next",           oedit_next          },
    { "persist",        oedit_persist       },
    { "prev",           oedit_prev          },
    { "removetype",     oedit_removetype    },
    { "scriptkwd",      oedit_skeywds       },
    { "short",          oedit_short         },
    { "show",           oedit_show          },
    { "sign",           oedit_sign          },
    { "timer",          oedit_timer         },
    { "type",           oedit_type          },
    { "v0",             oedit_value0        },
    { "v1",             oedit_value1        },
    { "v2",             oedit_value2        },
    { "v3",             oedit_value3        },
    { "v4",             oedit_value4        },
    { "v5",             oedit_value5        },
    { "v6",             oedit_value6        },
    { "v7",             oedit_value7        },
    { "varclear",       oedit_varclear      },
    { "varset",         oedit_varset        },
    { "waypoints",      oedit_waypoints     },
    { "wear",           oedit_wear          },
    { "weight",         oedit_weight        },

    /* Type-specific subcommands */
    { "armor",          oedit_armor         },
    { "bodypart",       oedit_bodypart      },
    { "book",           oedit_book          },
    { "cart",           oedit_cart          },
    { "compass",        oedit_compass       },
    { "container",      oedit_container     },
    { "corpse",         oedit_corpse        },
    { "drink",          oedit_drink         },
    { "food",           oedit_food          },
    { "furniture",      oedit_furniture     },
    { "herb",           oedit_herb          },
    { "ink",            oedit_ink           },
    { "instrument",     oedit_instrument    },
    { "jewelry",        oedit_jewelry       },
    { "light",          oedit_light         },
    { "map",            oedit_map           },
    { "mist",           oedit_mist          },
    { "money",          oedit_money         },
    { "page",           oedit_page          },
    { "portal",         oedit_portal        },
    { "scroll",         oedit_scroll        },
    { "seed",           oedit_seed          },
    { "sextant",        oedit_sextant       },
    { "ship",           oedit_ship          },
    { "tattoo",         oedit_tattoo        },
    { "telescope",      oedit_telescope     },
    { "tool",           oedit_tool          },
    { "trade",          oedit_trade         },
    { "wand",           oedit_wand          },
    { "weapon",         oedit_weapon        },
    { "weaponcon",      oedit_weaponcon     },
    { NULL,             0,                  }
};

/*
 * Object Editor Definition
 */
static const OLC_EDITOR_DEF oedit_def = {
    .name           = "OEdit",
    .editor_type    = ED_OBJECT,
    .cmd_table      = oedit_table,
    .show_fn        = oedit_show,
    .tabs           = {
        .count = 5,
        .tabs = {
            { "General",    "Gen", oedit_show_general_tab },
            { "Properties", "Prp", oedit_show_properties_tab },
            { "Affects",    "Aff", oedit_show_affects_tab },
            { "Scripts",    "Scr", oedit_show_scripts_tab },
            { "Type",       "Typ", oedit_show_type_tab },
        }
    },
    .theme          = &olc_theme_entity,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = oedit_get_area,
    .audit_changes  = true,
};

/*
 * Object Editor Interpreter — delegates to framework.
 */
void oedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &oedit_def);
}

/*
 * Object Editor Entry Point
 */
void do_oedit(CHAR_DATA *ch, char *argument)
{
    OBJ_INDEX_DATA *pObj;
    AREA_DATA *pArea;
    char arg1[MAX_STRING_LENGTH];
    long value;

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg1);

    if (arg1[0] != '\0' && str_cmp(arg1, "create"))
    {
        WNUM wnum;
        AREA_DATA *context = ch->in_room ? ch->in_room->area : NULL;
        if (!parse_widevnum(arg1, context, &wnum)) {
            send_to_char("OEdit: Invalid widevnum format. Use vnum, #vnum or area#vnum.\n\r", ch);
            return;
        }

        if (!(pObj = get_obj_index(wnum.pArea, wnum.vnum)))
        {
            send_to_char("OEdit:  That vnum does not exist.\n\r", ch);
            return;
        }

        if (!has_access_area(ch, pObj->area))
        {
            send_to_char("Insufficient security to edit object - action logged.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &oedit_def, (void *)pObj, true);
    }
    else if (!str_cmp(arg1, "create"))
    {
        value = atol(argument);

        if (argument[0] != '\0')
        {
            pArea = get_vnum_area(value);

            if (!pArea)
            {
                send_to_char("OEdit:  That vnum is not assigned an area.\n\r", ch);
                return;
            }

            if (!has_access_area(ch, pArea))
            {
                send_to_char("Insufficient security to edit object - action logged.\n\r", ch);
                return;
            }
        }

        if (oedit_create(ch, argument))
            olc_editor_enter(ch, &oedit_def, ch->desc->pEdit, true);
    }
}


/*
 * ========================================================================
 * Tab Show Functions
 * ========================================================================
 */

static void oedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&oedit_def);
    char buf[MAX_STRING_LENGTH];

    olc_display_string(ctx, theme, "Name:", "name", pObj->name);
    olc_display_infof(ctx, theme, "Area:         %s[%s%7ld%s] %s%s{x",
        theme->label, theme->value,
        !pObj->area ? -1L : pObj->area->uid,
        theme->label,
        theme->value,
        !pObj->area ? "No Area" : pObj->area->name);
    olc_display_number(ctx, theme, "Vnum:", NULL, pObj->vnum);

    /* Primary type + secondary types */
    snprintf(buf, sizeof(buf), "%s", flag_string(type_flags, pObj->item_type));
    for (int t = 0; t < ITEM__MAX; t++) {
        if (t == pObj->item_type) continue;
        if (TBIT_TST(pObj->type_flags, t)) {
            int len = strlen(buf);
            snprintf(buf + len, sizeof(buf) - len, " {D+{x %s", flag_string(type_flags, t));
        }
    }
    olc_display_string(ctx, theme, "Type:", "type", buf);

    olc_display_bool(ctx, theme, "Persist:", "persist", pObj->persist);
    olc_display_number(ctx, theme, "Level:", "level", pObj->level);
    olc_display_string(ctx, theme, "Imp Sig:", NULL, pObj->imp_sig);
    olc_display_string(ctx, theme, "Creator Sig:", NULL, pObj->creator_sig);
    olc_display_string(ctx, theme, "Script Kwds:", "scriptkwd", pObj->skeywds);

    olc_display_hr(ctx, theme);

    olc_display_string(ctx, theme, "Short Desc:", "short", pObj->short_descr);
    olc_display_text(ctx, theme, "Long Desc:", "long", pObj->description);
    olc_display_text(ctx, theme, "Description:", "description", pObj->full_description);
    olc_display_text(ctx, theme, "Comments:", "comments", pObj->comments);
}

static void oedit_show_properties_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&oedit_def);
    char buf[MAX_STRING_LENGTH];
    bool legacy_material = false;
    const char *material_name_display = material_resolve_name(pObj->material, &legacy_material);

    olc_display_string(ctx, theme, "Extra Flags:", "extra",
        bitvector_string(4,
            pObj->extra[0], extra_flags,
            pObj->extra[1], extra2_flags,
            pObj->extra[2], extra3_flags,
            pObj->extra[3], extra4_flags));

    olc_display_number(ctx, theme, "Timer:", "timer", pObj->timer);
    olc_display_string(ctx, theme, "Material:", "material", material_name_display);
    if (legacy_material && IS_IMMORTAL(ch))
        olc_display_infof(ctx, theme, "{R[Legacy] Material '%s' is not in materials.json; fallback string is active.{x", pObj->material);
    olc_display_number(ctx, theme, "Condition:", "condition", pObj->condition);
    olc_display_string(ctx, theme, "Fragility:", "fragility", fragile_table[pObj->fragility].name);
    olc_display_number(ctx, theme, "Allwd Fixed:", "allowedfixed", pObj->times_allowed_fixed);
    olc_display_number(ctx, theme, "Weight:", "weight", pObj->weight);
    olc_display_number(ctx, theme, "Cost:", "cost", pObj->cost);
    olc_display_number(ctx, theme, "Points:", NULL, pObj->points);

    olc_display_hr(ctx, theme);

    olc_display_flags(ctx, theme, "Wear Flags:", "wear", wear_flags, pObj->wear_flags);

    if (pObj->lock) {
        OBJ_INDEX_DATA *lock_key = (pObj->lock->key_wnum.pArea && pObj->lock->key_wnum.vnum > 0)
            ? get_obj_index(pObj->lock->key_wnum.pArea, pObj->lock->key_wnum.vnum) : NULL;

        olc_display_section(ctx, theme, "Lock State");
        olc_display_vnum(ctx, theme, "Key:", "lock key",
            pObj->lock->key_wnum.vnum,
            lock_key ? lock_key->short_descr : NULL);
        olc_display_flags(ctx, theme, "Flags:", "lock flags", lock_flags, pObj->lock->flags);
        olc_display_percent(ctx, theme, "Pick Chance:", "lock pick", pObj->lock->pick_chance, 1);
    }

    if (pObj->extra_descr) {
        EXTRA_DESCR_DATA *ed;

        buf[0] = '\0';
        for (ed = pObj->extra_descr; ed; ed = ed->next) {
            int len = strlen(buf);
            snprintf(buf + len, sizeof(buf) - len, "[%s] ", ed->keyword);
        }
        olc_display_string(ctx, theme, "Ex Desc Kwd:", "ed", buf);
    }
}

static void oedit_show_affects_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&oedit_def);
    char buf[MAX_STRING_LENGTH];
    AFFECT_DATA *paf;
    CATALYST_DATA *cat;
    SPELL_DATA *spell;
    int cnt;

    /* TO_OBJECT affects */
    cnt = 0;
    for (paf = pObj->affected; paf; paf = paf->next) {
        if (paf->where == TO_OBJECT) {
            if (cnt == 0) {
                olc_display_section(ctx, theme, "Affects");
                snprintf(buf, sizeof(buf), "  {Y%-6s %-20s %-10s %-10s{x", "Number", "Affects", "Modifier", "Random");
                add_buf(ctx->buffer, buf);
                add_buf(ctx->buffer, "\n\r");
                snprintf(buf, sizeof(buf), "  {Y%-6s %-20s %-10s %-10s{x", "------", "-------", "--------", "------");
                add_buf(ctx->buffer, buf);
                add_buf(ctx->buffer, "\n\r");
            }
            snprintf(buf, sizeof(buf), "  {B[{W%4d{B] {%c%-20s{x %-20d %d%%\n\r",
                cnt,
                (paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) ? 'Y' : 'G',
                affect_loc_name(paf->location),
                paf->modifier,
                paf->random);
            add_buf(ctx->buffer, buf);
            cnt++;
        }
    }

    /* TO_IMMUNE / TO_RESIST / TO_VULN */
    cnt = 0;
    for (paf = pObj->affected; paf; paf = paf->next) {
        if (paf->where == TO_IMMUNE || paf->where == TO_RESIST || paf->where == TO_VULN) {
            char *irv;
            if (paf->where == TO_IMMUNE)
                irv = "Wimmunity";
            else if (paf->where == TO_VULN)
                irv = "Rvulnerability";
            else
                irv = "Gresistance";

            if (cnt == 0) {
                olc_display_section(ctx, theme, "Immunities / Resistances / Vulnerabilities");
                snprintf(buf, sizeof(buf), "  {C%-6s %-15s %-15s %-10s{x", "Number", "Adds", "Modifier", "Random");
                add_buf(ctx->buffer, buf);
                add_buf(ctx->buffer, "\n\r");
                snprintf(buf, sizeof(buf), "  {C%-6s %-15s %-15s %-10s{x", "------", "-------", "--------", "------");
                add_buf(ctx->buffer, buf);
                add_buf(ctx->buffer, "\n\r");
            }
            snprintf(buf, sizeof(buf), "  {B[{W%4d{B] {%-16s{x %-15s %d%%\n\r",
                cnt, irv, imm_bit_name(paf->bitvector), paf->random);
            add_buf(ctx->buffer, buf);
            cnt++;
        }
    }

    /* Spells */
    if (pObj->spells) {
        olc_display_section(ctx, theme, "Spells");

        snprintf(buf, sizeof(buf), "  {g%-6s %-20s %-10s %-6s{x", "Number", "Spell", "Level", "Random");
        add_buf(ctx->buffer, buf);
        add_buf(ctx->buffer, "\n\r");
        snprintf(buf, sizeof(buf), "  {g%-6s %-20s %-10s %-6s{x", "------", "-----", "-----", "------");
        add_buf(ctx->buffer, buf);
        add_buf(ctx->buffer, "\n\r");

        cnt = 0;
        for (spell = pObj->spells; spell; spell = spell->next, cnt++) {
            snprintf(buf, sizeof(buf), "  {B[{W%4d{B]{x %-20s %-10d %d%%\n\r",
                cnt, skill_table[spell->sn].name, spell->level, spell->repop);
            buf[2] = UPPER(buf[2]);
            add_buf(ctx->buffer, buf);
        }
    }

    /* Catalysts */
    if (pObj->catalyst) {
        olc_display_section(ctx, theme, "Catalysts");

        snprintf(buf, sizeof(buf), "  {m%-6s %-20s %-10s %-6s %-6s %-11s{x",
            "Number", "Type", "Strength", "Amount", "Random", "Script Name");
        add_buf(ctx->buffer, buf);
        add_buf(ctx->buffer, "\n\r");
        snprintf(buf, sizeof(buf), "  {m%-6s %-20s %-10s %-6s %-6s %-11s{x",
            "------", "----", "--------", "------", "------", "-----------");
        add_buf(ctx->buffer, buf);
        add_buf(ctx->buffer, "\n\r");

        cnt = 0;
        for (cat = pObj->catalyst; cat; cat = cat->next, cnt++) {
            char line_colour = (cat->where == TO_CATALYST_ACTIVE) ? 'W' : 'x';
            char *name = (IS_NULLSTR(cat->custom_name)) ? "---" : cat->custom_name;

            if (cat->modifier < 0)
                snprintf(buf, sizeof(buf), "  {M[{W%4d{M]{%c %-20s %-10d {Wsource{%c %d%% %s{x\n\r",
                    cnt, line_colour,
                    flag_string(catalyst_types, cat->type), cat->level,
                    line_colour, cat->random, name);
            else
                snprintf(buf, sizeof(buf), "  {M[{W%4d{M]{%c %-20s %-10d %-6d %d%% %s{x\n\r",
                    cnt, line_colour,
                    flag_string(catalyst_types, cat->type), cat->level,
                    cat->modifier, cat->random, name);

            buf[2] = UPPER(buf[2]);
            add_buf(ctx->buffer, buf);
        }
    }

    /* Waypoints */
    if (list_size(pObj->waypoints) > 0) {
        int wcnt = 0;
        ITERATOR wit;
        WAYPOINT_DATA *wp;
        WILDS_DATA *wilds;

        olc_display_section(ctx, theme, "Cartographer Waypoints");

        add_buf(ctx->buffer, "  {B     [     Wilderness     ] [ South ] [  East ] [        Name        ]{x\n\r");
        add_buf(ctx->buffer, "  {B======================================================================={x\n\r");

        iterator_start(&wit, pObj->waypoints);
        while ((wp = (WAYPOINT_DATA *)iterator_nextdata(&wit))) {
            wilds = get_wilds_from_uid(NULL, wp->w);
            char *wname = wilds ? wilds->name : "{D(null){x";
            int wwidth = get_colour_width(wname) + 20;

            snprintf(buf, sizeof(buf), "  {B%3d{b)  {W%-*.*s    {G%5d     %5d    {Y%s{x\n\r",
                ++wcnt, wwidth, wwidth, wname, wp->y, wp->x, wp->name);
            add_buf(ctx->buffer, buf);
        }
        iterator_stop(&wit);
    }
}

static void oedit_show_scripts_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&oedit_def);

    olc_display_scripts(ctx, theme, pObj->progs, PRG_OPROG,
        "ObjProg Vnum", "addoprog", "deloprog");

    olc_display_vars(ctx, theme, pObj->index_vars, "varset", "varclear");
}

static void oedit_show_type_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)pEdit;

    oedit_show_type_data(pObj, ctx->buffer);
}

/*
 * ========================================================================
 * Main show function — dispatches to active tab
 * ========================================================================
 */

OEDIT(oedit_show)
{
    OBJ_INDEX_DATA *pObj;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&oedit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    EDIT_OBJ(ch, pObj);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "OEdit", pObj->short_descr,
        formatf("%ld", pObj->vnum), &oedit_def);

    /* Dispatch to active tab's show function */
    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        for (int i = 0; i < oedit_def.tabs.count; i++) {
            if (oedit_def.tabs.tabs[i].show_fn)
                oedit_def.tabs.tabs[i].show_fn(ch, ctx, (void *)pObj);
        }
    } else if (tab >= 0 && tab < oedit_def.tabs.count && oedit_def.tabs.tabs[tab].show_fn) {
        oedit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)pObj);
    } else {
        oedit_show_general_tab(ch, ctx, (void *)pObj);
    }

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

OEDIT(oedit_addaffect)
{
    long value;
    OBJ_INDEX_DATA *pObj;
    AFFECT_DATA *pAf, *pAf_tmp;
    int pMod;
    bool pAdd = false;
    char loc[MAX_STRING_LENGTH];
    char mod[MAX_STRING_LENGTH];
    char randm[MAX_STRING_LENGTH];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    argument = one_argument(argument, loc);
    argument = one_argument(argument, mod);
    argument = one_argument(argument, randm);

    if (loc[0] == '\0'
    || mod[0] == '\0'
    || randm[0] == '\0'
    || !is_number(randm)
    || !is_number(mod))
    {
    send_to_char("Syntax:  addaffect [location] [#xmod] [#rand]\n\r", ch);
    return false;
    }

    if ((value = flag_value(apply_flags, loc)) == NO_FLAG) /* Hugin */
    {
        send_to_char("Valid affects are:\n\r", ch);
    show_help(ch, "apply");
    return false;
    }

    for (pAf = pObj->affected; pAf != NULL; pAf = pAf->next)
    {
    if (pAf->where == TO_OBJECT && pAf->location == value)
    {
        sprintf(buf, "There's already a %s modifier on that item.\n\r",
            flag_string(apply_flags, value));
        send_to_char(buf, ch);
        return false;
    }
    }

    switch(value)
    {
        case APPLY_HIT:
    case APPLY_MANA:
        pMod = (int) atoi(mod)/10;
        if (pMod == 0) pMod = 1;
        if (atoi(mod) < 0) pAdd = true;
        break;
    case APPLY_MOVE:
        pMod = (int) atoi(mod)/20;
        if (pMod == 0) pMod = 1;
        if (atoi(mod) < 0) pAdd = true;
        break;
    case APPLY_DEX:
    case APPLY_WIS:
    case APPLY_INT:
    case APPLY_STR:
    case APPLY_CON:
        pMod = atoi(mod);
        if (atoi(mod) < 0) pAdd = true;
        break;
    case APPLY_AC:
        pMod = (int) atoi(mod)/10;
        if (pMod == 0) pMod = 1;
        if (atoi(mod) > 0) pAdd = true;
        break;
    case APPLY_HITROLL:
    case APPLY_DAMROLL:
        pMod = (int) atoi(mod)/2;
        if (pMod == 0) pMod = 1;
        if (atoi(mod) < 0) pAdd = true;
        break;
    default:
        pMod = 1;
        pAdd = false;
        break;
    }

    /*
     * Modify based on random. This prevents people adding 123123
     * negative affects which dont ever actually repop on the item.
     */
    if (pAdd)
    {
        if (atoi(randm) < 10) pMod = 0;
        else if (atoi(randm) < 25) pMod = (int) pMod/4;
        else if (atoi(randm) < 50) pMod = (int) pMod/2;
        else if (atoi(randm) < 80) pMod = (int) pMod*4/5;
    }

    pMod = abs(pMod);

    if (!pAdd && (pObj->points - pMod) < 0)
    {
        send_to_char("You've already added enough positive affects.\n\r", ch);
        return false;
    }

    if (!pAdd)
        pObj->points -= pMod;
    else
        pObj->points += pMod;

    pAf             =   new_affect();
    pAf->next	    =   NULL;
    pAf->location   =   value;
    pAf->modifier   =   atoi(mod);
    pAf->where	    =   TO_OBJECT;
    pAf->type       =   -1;
    pAf->duration   =   -1;
    pAf->bitvector  =   0;
    pAf->level      =	pObj->level;
    pAf->random	    =   atoi(randm);

    if (!pObj->affected)
    pObj->affected = pAf;
    else
    {
    for (pAf_tmp = pObj->affected; pAf_tmp->next != NULL; pAf_tmp = pAf_tmp->next)
        ;

        pAf_tmp->next = pAf;
    }

    send_to_char("Affect added.\n\r", ch);
    return true;
}

OEDIT(oedit_addimmune)
{
    long value;
    OBJ_INDEX_DATA *pObj;
    AFFECT_DATA *pAf, *pAf_tmp;
    int pMod;
    int where;
    bool pAdd = false;
    char loc[MAX_STRING_LENGTH];
    char mod[MAX_STRING_LENGTH];
    char randm[MAX_STRING_LENGTH];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    argument = one_argument(argument, loc);
    argument = one_argument(argument, mod);
    argument = one_argument(argument, randm);

    if (loc[0] == '\0'
    || mod[0] == '\0'
    || randm[0] == '\0'
    || !is_number(randm))
    {
        send_to_char("Syntax:  addimmune [immune|resist|vuln] [bit] [#rand]\n\r", ch);
        return false;
    }

    where = flag_value(apply_types, loc);

    if( where != TO_IMMUNE && where != TO_RESIST && where != TO_VULN )
    {
        send_to_char("Syntax:  addimmune [immune|resist|vuln] [bit] [#rand]\n\r", ch);
        return false;
    }

    if( where == TO_IMMUNE )
    {
        if (!str_cmp(pObj->imp_sig, "none") && ch->tot_level < MAX_LEVEL)
        {
            send_to_char("You can't do this without an IMP's permission.\n\r", ch);
            return false;
        }
    }

    value = flag_value(imm_flags, mod);
    if( value == NO_FLAG || value == 0 )
    {
        send_to_char("Invalid bit flag\n\r"
              "Type '? imm' for a list of flags.\n\r", ch);
        return false;
    }

    if ( (value & (~value + 1)) != value )
    {
        send_to_char("You can only put one flag per immunity modifier.\n\r", ch);
        return false;
    }


    for (pAf = pObj->affected; pAf != NULL; pAf = pAf->next)
    {
        if ((pAf->where == TO_IMMUNE || pAf->where == TO_RESIST || pAf->where == TO_VULN) && ((pAf->bitvector & value) != 0))
        {
            sprintf(buf, "There's already an immunity modifier for %s on that item.\n\r",
                flag_string(imm_flags, value));
            send_to_char(buf, ch);
            return false;
        }
    }

    pMod = atoi(randm);

    switch(where)
    {
        case TO_IMMUNE:
            pMod = 5 * pMod / 2;
            break;
        case TO_RESIST:
            break;
        case TO_VULN:
            pAdd = true;
            break;
    }

    pMod = (pMod + 9) / 10;

    if (!pAdd && (pObj->points - pMod) < 0)
    {
        send_to_char("You've already added enough positive affects.\n\r", ch);
        return false;
    }

    if (!pAdd)
        pObj->points -= pMod;
    else
        pObj->points += pMod;

    pAf             =   new_affect();
    pAf->next	    =   NULL;
    pAf->location   =   APPLY_NONE;
    pAf->modifier   =   0;
    pAf->where	    =   where;
    pAf->type       =   -1;
    pAf->duration   =   -1;
    pAf->bitvector  =   value;
    pAf->level      =	pObj->level;
    pAf->random	    =   atoi(randm);

    if (!pObj->affected)
    pObj->affected = pAf;
    else
    {
    for (pAf_tmp = pObj->affected; pAf_tmp->next != NULL; pAf_tmp = pAf_tmp->next)
        ;

        pAf_tmp->next = pAf;
    }

    send_to_char("Immunity modifier added.\n\r", ch);
    return true;
}


OEDIT(oedit_addspell)
{
    OBJ_INDEX_DATA *pObj;
    char buf[MSL];
    char name[MSL];
    char level[MSL];
    char rand[MSL];
    SPELL_DATA *spell, *spell_tmp;
    int sn, i;
    bool restricted = true;
    bool spell_restricted = true;

    EDIT_OBJ(ch, pObj);

    if( ch->tot_level == MAX_LEVEL || has_imp_sig(NULL, pObj) )
        restricted = false;

    if( ch->tot_level == MAX_LEVEL )
        spell_restricted = false;

    if (restricted &&
        !(pObj->item_type == ITEM_SCROLL ||
        pObj->item_type == ITEM_WAND ||
        pObj->item_type == ITEM_STAFF ||
        pObj->item_type == ITEM_POTION ||
        pObj->item_type == ITEM_PILL ||
        pObj->item_type == ITEM_TATTOO ||
        pObj->item_type == ITEM_PORTAL))
    {
        send_to_char("You can't do this without an IMP's permission.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, name);
    argument = one_argument(argument, level);
    argument = one_argument(argument, rand);

    if (name[0] == '\0' || level[0] == '\0' || rand[0] == '\0'
    ||  !is_number(level) || !is_number(rand))
    {
    send_to_char("Syntax: addspell [spell name] [spell level] [random]\n\r", ch);
    return false;
    }

    if ((sn = skill_lookup(name)) == -1 || (spell_restricted && (skill_table[sn].spell_fun == spell_null)))
    {
        send_to_char("That's not a spell.\n\r", ch);
        return false;
    }

    if (pObj->item_type != ITEM_SCROLL
    &&  pObj->item_type != ITEM_PILL
    &&  pObj->item_type != ITEM_POTION
    &&  pObj->item_type != ITEM_TATTOO
    &&  pObj->item_type != ITEM_STAFF
    &&  pObj->item_type != ITEM_WAND)
    {
    for (spell_tmp = pObj->spells; spell_tmp != NULL; spell_tmp = spell_tmp->next)
    {
        if (spell_tmp->sn == sn)
        {
        send_to_char("That spell is already on the object.\n\r", ch);
        return false;
        }
    }
    }

    if ((i = atoi(level)) < 1 || i > MAX_LEVEL)
    {
    sprintf(buf, "Level range is 1-%d.\n\r", MAX_LEVEL);
    send_to_char(buf, ch);
    return false;
    }

    if ((i = atoi(rand)) < 1 || i > 100)
    {
    send_to_char("Random repop must be a percentage 1-100.\n\r", ch);
    return false;
    }

    spell 		= new_spell();
    spell->sn		= sn;
    spell->level	= atoi(level);
    spell->repop	= atoi(rand);
    spell->next = NULL;

    // Add to end of list
    if (pObj->spells == NULL)
    pObj->spells = spell;
    else
    {
    for (spell_tmp = pObj->spells; spell_tmp->next != NULL; spell_tmp = spell_tmp->next)
        ;

        spell_tmp->next = spell;
    }

    sprintf(buf, "Added spell %s, level %d, random %d.\n\r",
        skill_table[sn].name, spell->level, spell->repop);
    send_to_char(buf, ch);
    return true;
}

OEDIT(oedit_addskill)
{
    OBJ_INDEX_DATA *pObj;
    char buf[MSL];
    char name[MSL];
    char mod[MSL];
    char random[MSL];
    AFFECT_DATA *pAf, *pAf_tmp;
    int sn, i;

    EDIT_OBJ(ch, pObj);

    if (ch->tot_level < MAX_LEVEL && !has_imp_sig(NULL, pObj))
    {
    send_to_char("You can't do this without an IMP's permission.\n\r", ch);
    return false;
    }

    argument = one_argument(argument, name);
    argument = one_argument(argument, mod);
    argument = one_argument(argument, random);

    if (name[0] == '\0' || mod[0] == '\0' || random[0] == '\0' ||  !is_number(mod) || !is_number(random))
    {
    send_to_char("Syntax: addskill [skill name] [#modifier] [random]\n\r", ch);
    return false;
    }

    if ((sn = skill_lookup(name)) == -1)
    {
    send_to_char("That's not a skill.\n\r", ch);
    return false;
    }

    if ((i = atoi(mod)) < -100 || i > 100 || !i)
    {
    send_to_char("Skill modifier must be a positive (1 to 100) or negative (-1 to -100) percentage.\n\r", ch);
    return false;
    }

    if ((i = atoi(random)) < 1 || i > 100)
    {
    send_to_char("Random repop must be a percentage 1-100.\n\r", ch);
    return false;
    }

    pAf             =   new_affect();
    pAf->next	    =   NULL;
    pAf->location   =   APPLY_SKILL+sn;
    pAf->modifier   =   atoi(mod);
    pAf->where	    =   TO_OBJECT;
    pAf->type       =   -1;
    pAf->duration   =   -1;
    pAf->bitvector  =   0;
    pAf->level      =	pObj->level;
    pAf->random	    =   atoi(random);

    if (!pObj->affected)
    pObj->affected = pAf;
    else
    {
    for (pAf_tmp = pObj->affected; pAf_tmp->next != NULL; pAf_tmp = pAf_tmp->next)
        ;

        pAf_tmp->next = pAf;
    }

    sprintf(buf, "Added skill %s, percent mod %d%%, random %d.\n\r",
        skill_table[sn].name, pAf->modifier, pAf->random);
    send_to_char(buf, ch);
    return true;
}


OEDIT(oedit_addcatalyst)
{
    OBJ_INDEX_DATA *pObj;
    char buf[MSL];
    char type[MSL];
    char strength[MSL];
    char charges[MSL];
    char chance[MIL];
    char where[MIL];
    CATALYST_DATA *cat, *pCat;
    int t, s, c, n, w;

    EDIT_OBJ(ch, pObj);

    if (ch->tot_level < MAX_LEVEL && !has_imp_sig(NULL, pObj))
    {
    send_to_char("You can't do this without an IMP's permission.\n\r", ch);
    return false;
    }

    argument = one_argument(argument, type);
    argument = one_argument(argument, strength);
    argument = one_argument(argument, charges);
    argument = one_argument(argument, chance);
    argument = one_argument(argument, where);

    if (!type[0] || !strength[0] || !charges[0] || !chance[0]
    ||  !is_number(strength) || (!is_number(charges) && str_prefix(charges,"source")) || !is_number(chance))
    {
    send_to_char("Syntax: addcatalyst [type] [strength] [charges] [chance] [active] [name]\n\r", ch);
    return false;
    }

    if ((t = flag_value(catalyst_types,type)) == NO_FLAG)
    {
    send_to_char("That's not a catalyst type.\n\r", ch);
    return false;
    }

    s = atoi(strength);
    c = atoi(chance);
    w = (where[0] && !str_cmp(where, "active")) ? TO_CATALYST_ACTIVE : TO_CATALYST_DORMANT;

    if (s < 1 || s > CATALYST_MAXSTRENGTH)
    {
        sprintf(buf, "Valid strengths are from 1 to %d.\n\r", CATALYST_MAXSTRENGTH);
        send_to_char(buf, ch);
        return false;
    }

    if(!str_prefix(charges,"source"))
        n = -1;
    else if ((n = atoi(charges)) < 1) {
        send_to_char("Invalid charges.\n\r", ch);
        return false;
    }

    c = URANGE(1,c,100);

    for(cat = pObj->catalyst; cat; cat = cat->next) {
        if(cat->where == w && cat->type == t && cat->level == s && cat->random == c) {
            if(cat->modifier < 0 || n < 0)
                cat->modifier = -1;
            else
                cat->modifier += n;
            break;
        }
    }

    if(!cat) {
        pCat = new_catalyst();
        pCat->next = NULL;
        pCat->where = w;
        pCat->modifier = n;
        pCat->type = t;
        pCat->level = s;
        pCat->random = c;

        if( !IS_NULLSTR(argument) )
            pCat->custom_name = str_dup(argument);

        // Add to end of list
        if (!pObj->catalyst)
            pObj->catalyst = pCat;
        else {
            for (cat = pObj->catalyst; cat->next != NULL; cat = cat->next);
            cat->next = pCat;
        }
    }

    send_to_char("Added catalyst.\n\r", ch);
    return true;
}


OEDIT(oedit_delspell)
{
    OBJ_INDEX_DATA *pObj;
    SPELL_DATA *spell, *spell_prev;
    int i, n;

    EDIT_OBJ(ch, pObj);

    if (!is_number(argument))
    {
    send_to_char("Syntax: delspell [#]\n\r", ch);
    return false;
    }

    n = atoi(argument);
    i = 0;
    spell_prev = NULL;
    for (spell = pObj->spells; spell != NULL; spell = spell->next)
    {
    if (i == n)
        break;

    i++;
    spell_prev = spell;
    }

    if (spell == NULL)
    {
    send_to_char("That spell isn't on the object.\n\r", ch);
    return false;
    }

    // First one on the list
    if (!spell_prev)
    {
    pObj->spells = spell->next;
    free_spell(spell);
    }
    else
    {
    spell_prev->next = spell->next;
    free_spell(spell);
    }

    send_to_char("Spell removed.\n\r", ch);
    return true;
}


OEDIT(oedit_delcatalyst)
{
    OBJ_INDEX_DATA *pObj;
    CATALYST_DATA *catalyst, *catalyst_prev;
    int i, n;

    EDIT_OBJ(ch, pObj);

    if (!is_number(argument))
    {
    send_to_char("Syntax: delcatalyst [#]\n\r", ch);
    return false;
    }

    n = atoi(argument);
    i = 0;
    catalyst_prev = NULL;
    for (catalyst = pObj->catalyst; catalyst != NULL; catalyst = catalyst->next)
    {
    if (i == n)
        break;

    i++;
    catalyst_prev = catalyst;
    }

    if (catalyst == NULL)
    {
    send_to_char("That catalyst isn't on the object.\n\r", ch);
    return false;
    }

    // First one on the list
    if (!catalyst_prev)
    {
    pObj->catalyst = catalyst->next;
    free_catalyst(catalyst);
    }
    else
    {
    catalyst_prev->next = catalyst->next;
    free_catalyst(catalyst);
    }

    send_to_char("Catalyst removed.\n\r", ch);
    return true;
}


OEDIT(oedit_next)
{
    OBJ_INDEX_DATA *pObj;
    OBJ_INDEX_DATA *nextObj = NULL;
    long next_vnum;

    EDIT_OBJ(ch, pObj);

    next_vnum = pObj->vnum;

    next_vnum++;
    while (nextObj == NULL
    && next_vnum <= pObj->area->max_vnum)
    {
    nextObj = get_obj_index(pObj->area, next_vnum);
    next_vnum++;
    }

    if (nextObj == NULL)
    {
    send_to_char("No next object in area.\n\r", ch);
    }
    else
    {
    olc_editor_enter(ch, &oedit_def, (void *)nextObj, false);
    }
    return false;
}

OEDIT(oedit_waypoints)
{
    char buf[MSL];
    char arg[MIL];
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if( pObj->item_type != ITEM_MAP )
    {
        send_to_char("Only MAP objects can have waypoints.\n\r", ch);
        return false;
    }

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  waypoints list\n\r", ch);
        send_to_char("         waypoints add <wilds> <south> <east>[ <name>]\n\r", ch);
        send_to_char("         waypoints delete <#>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if( !str_prefix(arg, "list") )
    {
        if (list_size(pObj->waypoints) > 0)
        {
            int cnt = 0;
            ITERATOR wit;
            WAYPOINT_DATA *wp;
            WILDS_DATA *wilds;

            BUFFER *buffer = new_buf();

            add_buf(buffer, "{BCartographer Waypoints:{x\n\r\n\r");
            add_buf(buffer, "{B     [     Wilderness     ] [ South ] [  East ] [        Name        ]{x\n\r");
            add_buf(buffer, "{B======================================================================={x\n\r");

            iterator_start(&wit, pObj->waypoints);
            while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
            {
                wilds = get_wilds_from_uid(NULL, wp->w);

                char *wname = wilds ? wilds->name : "{D(null){x";

                int wwidth = get_colour_width(wname) + 20;

                sprintf(buf, "{B%3d{b)  {W%-*.*s    {G%5d     %5d    {Y%s{x\n\r",
                    ++cnt,
                    wwidth, wwidth, wname,
                    wp->y, wp->x, wp->name);

                add_buf(buffer, buf);
            }

            iterator_stop(&wit);

            add_buf(buffer, "\n\r");

            page_to_char(buffer->string, ch);

            free_buf(buffer);
        }
        else
            send_to_char("No waypoints to display.\n\r", ch);

        return false;
    }

    if( !str_prefix(arg, "add") )
    {
        char arg2[MIL];
        char arg3[MIL];
        char arg4[MIL];

        long uid;
        WILDS_DATA *wilds;
        int x, y;

        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);
        argument = one_argument(argument, arg4);

        if( !is_number(arg2) || !is_number(arg3) || !is_number(arg4) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        uid = atol(arg2);
        wilds = get_wilds_from_uid(NULL, uid);
        if( !wilds )
        {
            send_to_char("No such wilderness.\n\r", ch);
            return false;
        }

        y = atoi(arg3);
        x = atoi(arg4);

        if( y < 0 || y >= wilds->map_size_y )
        {
            sprintf(buf, "South coordinate is out of bounds.  Please limit from 0 to %d.\n\r", wilds->map_size_y - 1);
            send_to_char(buf, ch);
            return false;
        }

        if( x < 0 || x >= wilds->map_size_x )
        {
            sprintf(buf, "East coordinate is out of bounds.  Please limit from 0 to %d.\n\r", wilds->map_size_x - 1);
            send_to_char(buf, ch);
            return false;
        }

        WAYPOINT_DATA *wp = new_waypoint();

        free_string(wp->name);
        wp->name = nocolour(argument);
        wp->w = uid;
        wp->x = x;
        wp->y = y;

        if( !pObj->waypoints )
        {
            pObj->waypoints = new_waypoints_list();
        }

        list_appendlink(pObj->waypoints, wp);
        send_to_char("Waypoint added.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "delete") )
    {
        int value;

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        if( !IS_VALID(pObj->waypoints) )
        {
            send_to_char("There are no waypoints to delete.\n\r", ch);
            return false;
        }

        value = atoi(argument);
        if( value < 1 || value > list_size(pObj->waypoints) )
        {
            send_to_char("No such waypoint.\n\r", ch);
            return false;
        }

        list_remnthlink(pObj->waypoints, value, true);
        send_to_char("Waypoint deleted.\n\r", ch);
        return true;
    }

    oedit_waypoints(ch, "");
    return false;
}

OEDIT(oedit_lock)
{
    char arg[MIL];
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  lock add\n\r", ch);
        send_to_char("         lock remove\n\r", ch);
        send_to_char("         lock key [vnum]\n\r", ch);
        send_to_char("         lock key clear\n\r", ch);
        send_to_char("         lock flags [flags]\n\r", ch);
        send_to_char("         lock pick [0-100]\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if( !str_prefix(arg, "add") )
    {
        if( pObj->lock )
        {
            send_to_char("Object already has a lock state.\n\r", ch);
            return false;
        }

        // TODO: Add closeability to weapon_containers and drinkcontainers
        if( pObj->item_type != ITEM_CONTAINER &&
            pObj->item_type != ITEM_PORTAL &&
//			pObj->item_type != ITEM_WEAPON_CONTAINER &&
//			pObj->item_type != ITEM_DRINKCONTAINER &&
            pObj->item_type != ITEM_BOOK )
        {
            send_to_char("Invalid object type.\n\r", ch);
            send_to_char("Only the following types may have lock state added:\n\r", ch);
            send_to_char("{Y*{x CONTAINER\n\r", ch);
            send_to_char("{Y*{x PORTAL\n\r", ch);
//			send_to_char("{Y*{x WEAPON_CONTAINER\n\r", ch);
//			send_to_char("{Y*{x DRINKCONTAINER\n\r", ch);
            send_to_char("{Y*{x BOOK\n\r", ch);
            return false;
        }

        pObj->lock = new_lock_state();
        send_to_char("Lock State added.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "remove") )
    {
        if( !pObj->lock )
        {
            send_to_char("Object does not have a lock state.\n\r", ch);
            return false;
        }


        free_lock_state(pObj->lock);
        pObj->lock = NULL;

        send_to_char("Lock State removed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "key") )
    {
        if( !pObj->lock )
        {
            send_to_char("Object does not have a lock state.\n\r", ch);
            return false;
        }

        if( argument[0] == '\0' )
        {
            send_to_char("Syntax:  lock key [widevnum]\n\r", ch);
            send_to_char("         lock key clear\n\r", ch);
            return false;
        }

        if( !str_prefix(argument, "clear") )
        {
            memset(&pObj->lock->key_load, 0, sizeof(WNUM_LOAD));
            memset(&pObj->lock->key_wnum, 0, sizeof(WNUM));
            send_to_char("Lock State key cleared.\n\r", ch);
            return true;
        }

        WNUM key_wnum;
        AREA_DATA *context = olc_relative_widevnum_context(pObj->area, argument);
        if (!parse_widevnum(argument, context, &key_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        OBJ_INDEX_DATA *key = get_obj_index(key_wnum.pArea, key_wnum.vnum);
        if( !key )
        {
            send_to_char("That object does not exist.\n\r", ch);
            return false;
        }

        if( key->item_type != ITEM_KEY )
        {
            send_to_char("That object is not a key.\n\r", ch);
            return false;
        }

        pObj->lock->key_load.auid = key_wnum.pArea ? key_wnum.pArea->uid : 0;
        pObj->lock->key_load.vnum = key_wnum.vnum;
        pObj->lock->key_wnum = key_wnum;
        send_to_char("Lock State key set.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "flags") )
    {
        if( !pObj->lock )
        {
            send_to_char("Object does not have a lock state.\n\r", ch);
            return false;
        }

        int value = flag_value(lock_flags, argument);

        if( value == NO_FLAG )
        {
            send_to_char("Syntax:  lock flags [flags]\n\r", ch);
            send_to_char("See \"? lock\" for list of flags\n\r\n\r", ch);
            show_help(ch, "lock");
            return false;
        }

        pObj->lock->flags ^= value;
        send_to_char("Lock State flags changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "pick") )
    {
        if( !pObj->lock )
        {
            send_to_char("Object does not have a lock state.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Pick chance must be from 0 to 100.\n\r", ch);
            return false;
        }

        pObj->lock->pick_chance = value;
        send_to_char("Lock State pick chance set.\n\r", ch);
        return true;
    }

    oedit_lock(ch, "");
    return false;
}

OEDIT(oedit_persist)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);


    if (!str_cmp(argument,"on")) {
        if (!str_cmp(pObj->imp_sig, "none") && ch->tot_level < MAX_LEVEL) {
            send_to_char("You can't do this without an IMP's permission.\n\r", ch);
            return false;
        }

        pObj->persist = true;
        use_imp_sig(NULL, pObj);
        send_to_char("Persistance enabled.\n\r", ch);
    } else if (!str_cmp(argument,"off")) {
        pObj->persist = false;
        send_to_char("Persistance disabled.\n\r", ch);
    } else {
        send_to_char("Usage: persist on/off\n\r", ch);
        return false;
    }

    return true;
}

OEDIT(oedit_prev)
{
    OBJ_INDEX_DATA *pObj;
    OBJ_INDEX_DATA *prevObj = NULL;
    long prev_vnum;

    EDIT_OBJ(ch, pObj);

    prev_vnum = pObj->vnum;

    prev_vnum--;
    while (prevObj == NULL
    && prev_vnum >= pObj->area->min_vnum)
    {
    prevObj = get_obj_index(pObj->area, prev_vnum);
    prev_vnum--;
    }

    if (prevObj == NULL)
    {
    send_to_char("No previous object in area.\n\r", ch);
    }
    else
    {
    olc_editor_enter(ch, &oedit_def, (void *)prevObj, false);
    }
    return false;
}


OEDIT(oedit_delaffect)
{
    OBJ_INDEX_DATA *pObj;
    AFFECT_DATA *pAf;
    AFFECT_DATA *pAf_prev;
    AFFECT_DATA *pAf_next;
    char affect[MAX_STRING_LENGTH];
    int  value;
    //int  cnt = 0;

    EDIT_OBJ(ch, pObj);

    one_argument(argument, affect);

    if (!is_number(affect) || affect[0] == '\0')
    {
    send_to_char("Syntax:  delaffect [#xaffect]\n\r", ch);
    return false;
    }

    value = atoi(affect);

    if (value < 0)
    {
    send_to_char("Only non-negative affect-numbers allowed.\n\r", ch);
    return false;
    }

    if (!(pAf = pObj->affected))
    {
    send_to_char("OEdit:  Non-existant affect.\n\r", ch);
    return false;
    }

    pAf_prev = NULL;
    for(;pAf;pAf_prev = pAf, pAf = pAf_next)
    {
        pAf_next = pAf->next;

        if( pAf->where == TO_OBJECT )
        {
            if( --value < 0 )
            {
                if( pAf_prev == NULL )
                    pObj->affected = pAf_next;
                else
                    pAf_prev->next = pAf_next;

                free_affect(pAf);
                send_to_char("Affect removed.\n\r", ch);
                return true;
            }
        }

    }

    send_to_char("No such affect.\n\r", ch);
    return false;
}

OEDIT(oedit_delimmune)
{
    OBJ_INDEX_DATA *pObj;
    AFFECT_DATA *pAf;
    AFFECT_DATA *pAf_prev;
    AFFECT_DATA *pAf_next;
    char affect[MAX_STRING_LENGTH];
    int  value;
    //int  cnt = 0;

    EDIT_OBJ(ch, pObj);

    one_argument(argument, affect);

    if (!is_number(affect) || affect[0] == '\0')
    {
    send_to_char("Syntax:  delimmune [#xaffect]\n\r", ch);
    return false;
    }

    value = atoi(affect);

    if (value < 0)
    {
    send_to_char("Only non-negative affect-numbers allowed.\n\r", ch);
    return false;
    }

    if (!(pAf = pObj->affected))
    {
    send_to_char("OEdit:  Non-existant affect.\n\r", ch);
    return false;
    }

    pAf_prev = NULL;
    for(;pAf;pAf_prev = pAf, pAf = pAf_next)
    {
        pAf_next = pAf->next;

        if( pAf->where == TO_IMMUNE || pAf->where == TO_RESIST || pAf->where == TO_VULN )
        {
            if( --value < 0 )
            {
                if( pAf_prev == NULL )
                    pObj->affected = pAf_next;
                else
                    pAf_prev->next = pAf_next;

                free_affect(pAf);
                send_to_char("Immunity modifier removed.\n\r", ch);
                return true;
            }
        }

    }

    send_to_char("No such immunity modifier.\n\r", ch);
    return false;
}

OEDIT(oedit_name)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_string(ch, argument, "Name", NULL, &pObj->name,
        OLC_STR_DEFAULT, NULL, NULL);
}


OEDIT(oedit_sign)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (ch->tot_level < MAX_LEVEL)
    {
    send_to_char("This is not for you to do.\n\r" , ch);
    return false;
    }

    free_string(pObj->imp_sig);
    pObj->imp_sig = str_dup(ch->name);

    send_to_char("Object signed.\n\r", ch);
    return true;
}

OEDIT(oedit_skeywds)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_string(ch, argument, "Script Keywords", NULL, &pObj->skeywds,
        OLC_STR_CLEARABLE, NULL, NULL);
}

OEDIT(oedit_varset)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    return olc_varset(&pObj->index_vars, ch, argument, false);
}

OEDIT(oedit_varclear)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    return olc_varclear(&pObj->index_vars, ch, argument, false);
}


OEDIT(oedit_short)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  short [string]\n\r", ch);
    return false;
    }

    free_string(pObj->short_descr);
    pObj->short_descr = str_dup(argument);

    if (pObj->area)
        SET_BIT(pObj->area->area_flags, AREA_CHANGED);

    send_to_char("Short description set.\n\r", ch);

    if (IS_SET(ch->act[0], PLR_AUTOSETNAME))
    {
    free_string(pObj->name);
    pObj->name = short_to_name(pObj->short_descr);
    send_to_char("Name keywords set.\n\r", ch);
    }
    return true;
}


OEDIT(oedit_long)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  long [string]\n\r", ch);
    return false;
    }

    strcat(argument, "{x");

    free_string(pObj->description);
    pObj->description = str_dup(argument);
    pObj->description[0] = UPPER(pObj->description[0]);

    send_to_char("Long description set.\n\r", ch);
    return true;
}


bool set_value(CHAR_DATA *ch, OBJ_INDEX_DATA *pObj, char *argument, int value)
{
    if (argument[0] == '\0')
    {
    set_obj_values(ch, pObj, -1, "");
    return false;
    }

    if (set_obj_values(ch, pObj, value, argument))
    return true;

    return false;
}


/* Finds the object and sets its value. */
bool oedit_values(CHAR_DATA *ch, char *argument, int value)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (set_value(ch, pObj, argument, value))
    {
        if (pObj->item_type == ITEM_WEAPON)
            set_weapon_dice(pObj);

        return true;
    }

    return false;
}


OEDIT(oedit_value0)
{
    if (oedit_values(ch, argument, 0))
        return true;

    return false;
}


OEDIT(oedit_value1)
{
    if (oedit_values(ch, argument, 1))
        return true;

    return false;
}


OEDIT(oedit_value2)
{
    if (oedit_values(ch, argument, 2))
        return true;

    return false;
}


OEDIT(oedit_value3)
{
    if (oedit_values(ch, argument, 3))
        return true;

    return false;
}


OEDIT(oedit_value4)
{
    if (oedit_values(ch, argument, 4))
        return true;

    return false;
}


OEDIT(oedit_value5)
{
    if (oedit_values(ch, argument, 5))
        return true;

    return false;
}


OEDIT(oedit_value6)
{
    if (oedit_values(ch, argument, 6))
        return true;

    return false;
}


OEDIT(oedit_value7)
{
    if (oedit_values(ch, argument, 7))
        return true;

    return false;
}


OEDIT(oedit_weight)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_number_i16(ch, argument, "Weight", NULL, &pObj->weight,
        0, INT_MAX, NULL, NULL);
}


OEDIT(oedit_cost)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0' || !is_number(argument))
    {
    send_to_char("Syntax:  cost [number]\n\r", ch);
    return false;
    }

    pObj->cost = atoi(argument);

    send_to_char("Cost set.\n\r", ch);
    return true;
}


OEDIT(oedit_create)
{
    OBJ_INDEX_DATA *pObj;
    OBJ_INDEX_DATA *temp_obj;
    AREA_DATA *pArea;
    long value;
    int iHash;
    long auto_vnum = 0;

    // Auto-vnum: if no argument or argument is 0, find next available vnum in current area
    if (argument[0] == '\0' || !str_cmp(argument, "0"))
    {
        pArea = ch->in_room->area;
        auto_vnum = 1;
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (temp_obj = pArea->obj_index_hash[iHash]; temp_obj; temp_obj = temp_obj->next)
            {
                if (temp_obj->vnum >= auto_vnum)
                    auto_vnum = temp_obj->vnum + 1;
            }
        }

        if (auto_vnum <= 0)
        {
            send_to_char("Unable to allocate a new object vnum.\n\r", ch);
            return false;
        }

        value = auto_vnum;
    }
    else
    {
        WNUM wnum;
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &wnum)) {
            send_to_char("Invalid vnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        value = wnum.vnum;
        pArea = wnum.pArea;
    }

    if (!pArea)
    {
        send_to_char("OEdit:  That vnum is not assigned an area.\n\r", ch);
        return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
        send_to_char("OEdit:  Vnum in an area you cannot build in.\n\r", ch);
        return false;
    }

    if (get_obj_index(pArea, value))
    {
        send_to_char("OEdit:  Object vnum already exists.\n\r", ch);
        return false;
    }

    pObj = new_obj_index();
    pObj->vnum = value;
    pObj->area = pArea;
    iHash = value % MAX_KEY_HASH;
    pObj->next = pArea->obj_index_hash[iHash];
    pArea->obj_index_hash[iHash] = pObj;
    ch->desc->pEdit = (void *)pObj;

    send_to_char("Object Created.\n\r", ch);
    SET_BIT(pObj->area->area_flags, AREA_CHANGED);
    free_string(pObj->creator_sig);
    pObj->creator_sig = str_dup(ch->name);
    return true;
}


OEDIT(oedit_ed)
{
    OBJ_INDEX_DATA *pObj;
    EXTRA_DESCR_DATA *ed;
    char command[MAX_INPUT_LENGTH];
    char keyword[MAX_INPUT_LENGTH];
    char copy_item[MAX_INPUT_LENGTH];
    EDIT_OBJ(ch, pObj);

    argument = one_argument(argument, command);
    argument = one_argument(argument, keyword);
    argument = one_argument(argument, copy_item);

    if (command[0] == '\0')
    {
        send_to_char("Syntax:  ed add [keyword]\n\r", ch);
        send_to_char("         ed delete [keyword]\n\r", ch);
        send_to_char("         ed edit [keyword]\n\r", ch);
        send_to_char("         ed format [keyword]\n\r", ch);
        send_to_char("         ed copy old_keyword new_keyword\n\r", ch);
        send_to_char("         ed environment [keyword]\n\r", ch);

        return false;
    }

    if (!str_cmp(command, "environment"))
    {
    if (keyword[0] == '\0')
    {
        send_to_char("Syntax:  ed environment [keyword]\n\r", ch);
        return false;
    }

    ed			=   new_extra_descr();
    ed->keyword		=   str_dup(keyword);
    ed->description		= NULL;
    ed->next		=   pObj->extra_descr;
    pObj->extra_descr	=   ed;

    send_to_char("Enviromental extra description added.\n\r", ch);

    return true;
    }

    if (!str_cmp(command, "copy"))
    {
        EXTRA_DESCR_DATA *ed2;

        if (keyword[0] == '\0' || copy_item[0] == '\0')
        {
            send_to_char("Syntax:  ed copy existing_keyword new_keyword\n\r", ch);
            return false;
        }

        for (ed = pObj->extra_descr; ed; ed = ed->next)
        {
            if (is_name(keyword, ed->keyword))
            break;
        }

        if (!ed)
        {
            send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
            return false;
        }

        ed2					= new_extra_descr();
        ed2->keyword		= str_dup(copy_item);
        ed2->next			= pObj->extra_descr;
        pObj->extra_descr	= ed2;
        ed2->description	= str_dup(ed->description);

        send_to_char("Done.\n\r", ch);

        return true;
    }

    if (!str_cmp(command, "add"))
    {
        if (keyword[0] == '\0')
        {
            send_to_char("Syntax:  ed add [keyword]\n\r", ch);
            return false;
        }

        ed					= new_extra_descr();
        ed->keyword			= str_dup(keyword);
        ed->next			= pObj->extra_descr;
        pObj->extra_descr	= ed;

        string_append(ch, &ed->description);

        return true;
    }

    if (!str_cmp(command, "edit"))
    {
        if (keyword[0] == '\0')
        {
            send_to_char("Syntax:  ed edit [keyword]\n\r", ch);
            return false;
        }

        for (ed = pObj->extra_descr; ed; ed = ed->next)
        {
            if (is_name(keyword, ed->keyword))
            break;
        }

        if (!ed)
        {
            send_to_char("OEdit:  Extra description keyword not found.\n\r", ch);
            return false;
        }

        if( !ed->description )
            ed->description = str_dup("");

        string_append(ch, &ed->description);

        return true;
    }

    if (!str_cmp(command, "delete"))
    {
        EXTRA_DESCR_DATA *ped = NULL;

        if (keyword[0] == '\0')
        {
            send_to_char("Syntax:  ed delete [keyword]\n\r", ch);
            return false;
        }

        for (ed = pObj->extra_descr; ed; ed = ed->next)
        {
            if (is_name(keyword, ed->keyword))
                break;
            ped = ed;
        }

        if (!ed)
        {
            send_to_char("OEdit:  Extra description keyword not found.\n\r", ch);
            return false;
        }

        if (!ped)
            pObj->extra_descr = ed->next;
        else
            ped->next = ed->next;

        free_extra_descr(ed);

        send_to_char("Extra description deleted.\n\r", ch);
        return true;
    }


    if (!str_cmp(command, "format"))
    {
        if (keyword[0] == '\0')
        {
            send_to_char("Syntax:  ed format [keyword]\n\r", ch);
            return false;
        }

        for (ed = pObj->extra_descr; ed; ed = ed->next)
        {
            if (is_name(keyword, ed->keyword))
                break;
        }

        if (!ed)
        {
            send_to_char("OEdit:  Extra description keyword not found.\n\r", ch);
            return false;
        }

        if( !ed->description )
        {
            send_to_char("OEdit:  Extra description is an environmental extra description.\n\r", ch);
            return false;
        }

        ed->description = format_string(ed->description);

        send_to_char("Extra description formatted.\n\r", ch);
        return true;
    }

    if (!str_cmp(command, "show"))
    {
        if (keyword[0] == '\0')
        {
            send_to_char("Syntax:  ed show [keyword]\n\r", ch);
            return false;
        }

        for (ed = pObj->extra_descr; ed; ed = ed->next)
        {
            if (is_name(keyword, ed->keyword))
                break;
        }

        if (!ed)
        {
            send_to_char("OEdit:  Extra description keyword not found.\n\r", ch);
            return false;
        }

        if (!ed->description)
        {
            send_to_char("OEdit:  Cannot show environmental extra description.\n\r", ch);
            return false;
        }

        page_to_char(ed->description, ch);

        return true;
    }

    oedit_ed(ch, "");
    return false;
}


OEDIT(oedit_extra)
{
    OBJ_INDEX_DATA *pObj;
    //int value;

    if (argument[0] != '\0')
    {
        EDIT_OBJ(ch, pObj);
        
        long extra[4];

        if (!bitvector_lookup(argument, 4, extra, extra_flags, extra2_flags, extra3_flags, extra4_flags))
        {
            send_to_char("Invalid extra flag.\n\r", ch);
            send_to_char("Type '? extra' for a list of flags.\n\r", ch);
            return false;
        }

        //TOGGLE_BIT(pObj->extra_flags, value);
        TOGGLE_BIT(pObj->extra[0], extra[0]);
        TOGGLE_BIT(pObj->extra[1], extra[1]);
        TOGGLE_BIT(pObj->extra[2], extra[2]);
        TOGGLE_BIT(pObj->extra[3], extra[3]);

        send_to_char("Extra flag toggled.\n\r", ch);
        return true;
    
    }

    send_to_char("Syntax:  extra [flag]\n\r"
          "Type '? extra' for a list of flags.\n\r", ch);
    return false;
}

/*
OEDIT(oedit_extra2)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_OBJ(ch, pObj);
    
    // We should be adding checks to individual flags.

    if (!has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL)
    {
        send_to_char("You can't do this without an IMP's permission.\n\r", ch);
        return false;
    }


    if ((value = flag_value(extra2_flags, argument)) != NO_FLAG)
    {
        TOGGLE_BIT(pObj->extra2_flags, value);

        if (has_imp_sig(NULL, pObj))
        use_imp_sig(NULL, pObj);

        send_to_char("Extra2 flag toggled.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax:  extra2 [flag]\n\r"
          "Type '? extra2' for a list of flags.\n\r", ch);
    return false;
}

OEDIT(oedit_extra3)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_OBJ(ch, pObj);

    // We should be adding checks to individual flags.

    if (!has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL)
    {
        send_to_char("You can't do this without an IMP's permission.\n\r", ch);
        return false;
    }


    if ((value = flag_value(extra3_flags, argument)) != NO_FLAG)
    {
        TOGGLE_BIT(pObj->extra3_flags, value);

        if (has_imp_sig(NULL, pObj))
        use_imp_sig(NULL, pObj);

        send_to_char("Extra3 flag toggled.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax:  extra3 [flag]\n\r"
          "Type '? extra3' for a list of flags.\n\r", ch);
    return false;
}

OEDIT(oedit_extra4)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_OBJ(ch, pObj);

    // We should be adding checks to individual flags.

    if (!has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL)
    {
        send_to_char("You can't do this without an IMP's permission.\n\r", ch);
        return false;
    }


    if ((value = flag_value(extra4_flags, argument)) != NO_FLAG)
    {
        TOGGLE_BIT(pObj->extra4_flags, value);

        if (has_imp_sig(NULL, pObj))
        use_imp_sig(NULL, pObj);

        send_to_char("Extra4 flag toggled.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax:  extra4 [flag]\n\r"
          "Type '? extra4' for a list of flags.\n\r", ch);
    return false;
}
*/

OEDIT(oedit_wear)
{
    OBJ_INDEX_DATA *pObj;
    int value, wear;

    if (argument[0] != '\0')
    {
    EDIT_OBJ(ch, pObj);

    value = flag_value(wear_flags, argument);

    wear = (pObj->wear_flags ^ value) & ~(ITEM_TAKE|ITEM_CONCEALS|ITEM_NO_SAC);

    if((wear & -wear) != wear) {
        send_to_char("You can't set an object to be worn in more than one spot at once.\n\r", ch);
        return false;
    }

        if ((flag_value(wear_flags, argument) == ITEM_WEAR_BACK)
    &&     pObj->item_type != ITEM_RANGED_WEAPON)
    {
        send_to_char("Only ranged weapons can be slung behind the back.\n\r", ch);
        return false;
    }

    if ((flag_value(wear_flags, argument) == ITEM_WEAR_SHOULDER)
    &&     pObj->item_type != ITEM_WEAPON_CONTAINER)
    {
        send_to_char("Only weapon containers can be worn on the shoulder.\n\r", ch);
        return false;
    }

    if ((value = flag_value(wear_flags, argument)) != NO_FLAG)
    {
        TOGGLE_BIT(pObj->wear_flags, value);

        send_to_char("Wear flag toggled.\n\r", ch);

        return true;
    }
    }

    send_to_char("Syntax:  wear [flag]\n\r"
          "Type '? wear' for a list of flags.\n\r", ch);
    return false;
}


OEDIT(oedit_type)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    if (argument[0] != '\0')
    {
        EDIT_OBJ(ch, pObj);

        if ((value = flag_value(type_flags, argument)) != NO_FLAG)
        {
            if ((value == ITEM_KEYRING ||
                 value == ITEM_BANK ||
                 value == ITEM_SHARECERT ||
                 value == ITEM_ROOM_DARKNESS ||
                 value == ITEM_ROOM_FLAME ||
                 value == ITEM_SMOKE_BOMB ||
                 value == ITEM_MONEY ||
                 value == ITEM_WITHERING_CLOUD ||
                 value == ITEM_ROOM_ROOMSHIELD ||
                 value == ITEM_CATALYST ||
                 value == ITEM_SHIP ||
                 value == ITEM_SHRINE) &&
                ch->tot_level < MAX_LEVEL)
            {
                send_to_char("Sorry, only an IMP can set that item-type.\n\r",ch);
                return false;
            }

            /* Free all existing type data and allocate new primary type */
            obj_index_set_primary_type(pObj, value);

            /* Type-specific defaults */
            if (value == ITEM_TELESCOPE && IS_TELESCOPE(pObj))
                TELESCOPE(pObj)->heading = -1;

            if( pObj->lock )
            {
                free_lock_state(pObj->lock);
                pObj->lock = NULL;
            }

            if( pObj->waypoints )
            {
                list_destroy(pObj->waypoints);
                pObj->waypoints = NULL;
            }

            send_to_char("Type set.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax:  type [flag]\n\r"
                "Type '? type' for a list of flags.\n\r", ch);
    return false;
}


OEDIT(oedit_addtype)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  addtype <type>\n\r"
                     "Adds a secondary type to this object.\n\r"
                     "Type '? type' for a list of types.\n\r", ch);
        return false;
    }

    if ((value = flag_value(type_flags, argument)) == NO_FLAG)
    {
        send_to_char("Invalid type. Type '? type' for a list.\n\r", ch);
        return false;
    }

    if (value == pObj->item_type)
    {
        send_to_char("That is already the primary type.\n\r", ch);
        return false;
    }

    if (!obj_index_can_add_item_type(pObj, value))
    {
        send_to_char("That type is not compatible with this object's current types.\n\r", ch);
        return false;
    }

    if (!obj_index_alloc_type_data(pObj, value))
    {
        send_to_char("That type is already present on this object.\n\r", ch);
        return false;
    }

    send_to_char("Secondary type added.\n\r", ch);
    return true;
}


OEDIT(oedit_removetype)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  removetype <type>\n\r"
                     "Removes a secondary type from this object.\n\r"
                     "Type '? type' for a list of types.\n\r", ch);
        return false;
    }

    if ((value = flag_value(type_flags, argument)) == NO_FLAG)
    {
        send_to_char("Invalid type. Type '? type' for a list.\n\r", ch);
        return false;
    }

    if (!obj_index_remove_type(pObj, value))
    {
        send_to_char("Cannot remove that type (either it's the primary type or not present).\n\r", ch);
        return false;
    }

    send_to_char("Secondary type removed.\n\r", ch);
    return true;
}


OEDIT(oedit_material)
{
    OBJ_INDEX_DATA *pObj;
    int num;
    char *name;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  material [string]\n\r", ch);
    return false;
    }

    if ((num = material_lookup(argument)) == -1)
    {
    send_to_char("Invalid material. Type '? material.'\n\r", ch);
    return false;
    }

    name = material_name(num);
    if (IS_NULLSTR(name))
    {
    send_to_char("Material exists but has no name; please fix it in matedit.\n\r", ch);
    return false;
    }

    free_string(pObj->material);
    pObj->material = str_dup(name);

    send_to_char("Material set.\n\r", ch);
    return true;
}


OEDIT(oedit_level)
{
    OBJ_INDEX_DATA *pObj;
    char buf[MAX_STRING_LENGTH];
    int armour;
    int armour_exotic;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0' || !is_number(argument))
    {
    send_to_char("Syntax:  level [number]\n\r", ch);
    return false;
    }

    pObj->level = atoi(argument);

    send_to_char("Level set.\n\r", ch);

    pObj->points = (int)pObj->level/10;
    sprintf(buf, "This object is now assigned {Y%d{x points.\n\r",
            pObj->points);
    send_to_char(buf, ch);

    /* auto setting weapon dice stuff */
    if (pObj->item_type == ITEM_WEAPON)
    {
        set_weapon_dice(pObj);
    send_to_char("Damage dice set.\n\r", ch);
    }


    /* auto setting armour stuff */
    if (pObj->item_type == ITEM_ARMOUR)
    {
        armour=(int) calc_obj_armour(pObj->level, pObj->value[4]);
    armour_exotic=(int) armour * .90;

    pObj->value[0] = armour;
    pObj->value[1] = armour;
    pObj->value[2] = armour;
    pObj->value[3] = armour_exotic;

    send_to_char("Armour class set.\n\r", ch);
    }

    return true;
}


OEDIT(oedit_condition)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_number_i16(ch, argument, "Condition",
        "Syntax:  condition [number]\n\r"
        "Where number can range from 0 (ruined) to 100 (perfect).\n\r",
        &pObj->condition, 0, 100, NULL, NULL);
}


OEDIT(oedit_fragility)
{
    OBJ_INDEX_DATA *pObj;
    bool set = false;

    if (argument[0] != '\0')
    {
    EDIT_OBJ(ch, pObj);

    if (!str_cmp(argument, "Solid"))
    {
        if (!str_cmp(pObj->imp_sig, "none")
        && ch->tot_level < MAX_LEVEL)
        {
        send_to_char("You can't do this without an IMP's "
            "permission.\n\r", ch);
        return false;
        }

        pObj->fragility = OBJ_FRAGILE_SOLID;
        set = true;
        use_imp_sig(NULL, pObj);
    }

    if (!str_cmp(argument, "Strong"))
    {
        pObj->fragility = OBJ_FRAGILE_STRONG;
        set = true;
    }

    if (!str_cmp(argument, "Normal"))
    {
        pObj->fragility = OBJ_FRAGILE_NORMAL;
        set = true;
    }

    if (!str_cmp(argument, "Weak"))
    {
        pObj->fragility = OBJ_FRAGILE_WEAK;
        set = true;
    }

    if (set)
    {
        send_to_char("Fragility set.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax:  fragility  Solid|Strong|Normal|Weak\n\r"
        "Fragility.\n\r",
        ch);
    return false;
}


OEDIT(oedit_allowed_fixed)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_number_i16(ch, argument, "Allowed Fixed",
        "Syntax:  allowedfixed [number]\n\r"
        "Number of times a person can fix the object.\n\r",
        &pObj->times_allowed_fixed, 0, 100, NULL, NULL);
}

OEDIT (oedit_addoprog)
{
    int tindex, value, slot;
    PROG_LIST *list;
    SCRIPT_DATA *code;
  OBJ_INDEX_DATA *pObj;
  char trigger[MAX_STRING_LENGTH];
  char phrase[MAX_STRING_LENGTH];
  char num[MAX_STRING_LENGTH];

  EDIT_OBJ(ch, pObj);
  argument=one_argument(argument, num);
  argument=one_argument(argument, trigger);
  argument=one_argument(argument, phrase);

  if (num[0] == '\0' || trigger[0] =='\0' || phrase[0] =='\0')
  {
        send_to_char("Syntax:   addoprog [widevnum] [trigger] [phrase]\n\r",ch);
        return false;
  }

    if ((tindex = trigger_index(trigger, PRG_OPROG)) < 0) {
    send_to_char("Valid flags are:\n\r",ch);
    show_help(ch, "oprog");
    return false;
    }

    value = tindex;//trigger_table[tindex].value;
    slot = trigger_table[tindex].slot;

    if(value == TRIG_SPELLCAST) {
        if( !str_cmp(phrase, "*") )
        {
            strcpy(phrase, "0");
        }
        else
        {
            int sn = skill_lookup(phrase);
            if(sn < 0 || skill_table[sn].spell_fun == spell_null) {
                send_to_char("Invalid spell for trigger.\n\r",ch);
                return false;
            }
            sprintf(phrase,"%d",sn);
        }
    }
    else if( value == TRIG_EXIT || value == TRIG_EXALL )
    {
        if( !str_cmp(phrase, "*") )
        {
            strcpy(phrase, "-1");
        }
        else
        {
            int door = parse_door(phrase);
            if( door < 0 ) {
                send_to_char("Invalid direction for exit/exall trigger.\n\r", ch);
                return false;
            }
            sprintf(phrase,"%d",door);
        }
    }


  WNUM script_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(pObj->area, num);
  if (!parse_widevnum(num, context, &script_wnum)) {
      send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
      return false;
  }

  if ((code = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_OPROG)) == NULL)
  {
        send_to_char("No such OBJProgram.\n\r",ch);
        return false;
  }

    // Make sure this has a list of progs!
    if(!pObj->progs) pObj->progs = new_prog_bank();

    if (edit_trigger_exists(pObj->progs, code, tindex, phrase)) {
        send_to_char("That trigger/phrase pair is already attached to that script on this object.\n\r", ch);
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

    list_appendlink(pObj->progs[slot], list);

  send_to_char("Oprog Added.\n\r",ch);
  return true;
}

OEDIT (oedit_deloprog)
{
    OBJ_INDEX_DATA *pObj;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int group_idx, trig_idx;
    PROG_GROUP groups[MAX_PROG_GROUPS];
    int num_groups;

    EDIT_OBJ(ch, pObj);

    if (!pObj->progs) {
        send_to_char("This object has no programs attached.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  deloprog <group#>\n\r", ch);
        send_to_char("         deloprog <group#> <trigger#>\n\r", ch);
        return false;
    }

    if (!is_number(arg1)) {
        send_to_char("Please specify a valid group number.\n\r", ch);
        return false;
    }

    group_idx = atoi(arg1);
    num_groups = prog_build_groups(pObj->progs, groups, MAX_PROG_GROUPS, PRG_OPROG);

    if (group_idx < 1 || group_idx > num_groups) {
        send_to_char("Invalid group number.\n\r", ch);
        return false;
    }

    PROG_GROUP *group = &groups[group_idx - 1];

    if (arg2[0] == '\0') {
        // Delete entire group (all triggers for this script)
        if (edit_delscript(pObj->progs, group->script)) {
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
        if (edit_deltrigger_specific(pObj->progs, group->script, entry->entry->trig_type, entry->entry->trig_phrase)) {
            send_to_char("Trigger removed from script group.\n\r", ch);
            return true;
        }
    }

    send_to_char("No such program or trigger found.\n\r", ch);
    return false;
}

OEDIT(oedit_desc)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &pObj->full_description, NULL, NULL);
}

OEDIT(oedit_comments)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &pObj->comments, NULL, NULL);
}
/*
OEDIT(oedit_update)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (ch->tot_level < MAX_LEVEL - 2)
    {
    send_to_char("Insufficient security to toggle update.\n\r", ch);
    return false;
    }

    if (pObj->update == true)
    {
    pObj->update = false;
    send_to_char("Update OFF.\n\r", ch);
    }
    else
    {
    pObj->update = true;
    send_to_char("Update ON.\n\r", ch);
    }

    return true;
}
*/
OEDIT(oedit_timer)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_number(ch, argument, "Timer",
        "Syntax: timer <#ticks>  (0 to 10000)\n\r",
        &pObj->timer, 0, 10000, NULL, NULL);
}
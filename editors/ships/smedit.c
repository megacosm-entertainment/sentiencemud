/***************************************************************************
 *  smedit.c — OLC Ship Module Template Editor                            *
 *                                                                         *
 *  In-game editor for SHIP_MODULE_INDEX definitions. Allows immortals    *
 *  to create, modify, and manage ship module templates that can be       *
 *  installed into ship hardpoint slots.                                   *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework.                          *
 ***************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include "../strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

extern bool ships_changed;
extern bool can_edit_ships(CHAR_DATA *ch);

/***************************************************************************
 * Permission & Change Tracking                                            *
 ***************************************************************************/

/**
 * smedit_check_perm - Custom permission check (reuses ship edit perms)
 *
 * @param ch     Character to check
 * @param pEdit  Module being edited (unused)
 * @return       true if character can edit ships/modules
 */
static bool smedit_check_perm(CHAR_DATA *ch, void *pEdit)
{
    (void)pEdit;
    return can_edit_ships(ch);
}

/**
 * smedit_mark_changed - Mark module data as needing save
 *
 * @param ch     Character who made the change
 * @param pEdit  SHIP_MODULE_INDEX being edited
 */
static void smedit_mark_changed(CHAR_DATA *ch, void *pEdit)
{
    (void)ch;
    SHIP_MODULE_INDEX *mod = (SHIP_MODULE_INDEX *)pEdit;
    if (mod && mod->area)
        SET_BIT(mod->area->area_flags, AREA_CHANGED);
    ships_changed = true;
}

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type smedit_table[] =
{
    { "?",          show_help           },
    { "ammo",       smedit_ammo         },
    { "armor",      smedit_armor        },
    { "cargo",      smedit_cargo        },
    { "commands",   show_commands       },
    { "create",     smedit_create       },
    { "crew",       smedit_crew         },
    { "damage",     smedit_damage       },
    { "desc",       smedit_desc         },
    { "domain",     smedit_domain       },
    { "flags",      smedit_flags        },
    { "hit",        smedit_hit          },
    { "list",       smedit_list         },
    { "name",       smedit_name         },
    { "operators",  smedit_operators    },
    { "range",      smedit_range        },
    { "reload",     smedit_reload       },
    { "show",       smedit_show         },
    { "size",       smedit_size         },
    { "skills",     smedit_skills       },
    { "speed",      smedit_speed        },
    { "turning",    smedit_turning      },
    { "type",       smedit_type         },
    { "weapflags",  smedit_weapflags    },
    { "weight",     smedit_weight       },
    { NULL,         0                   }
};

/***************************************************************************
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF smedit_def = {
    .name           = "SMEdit",
    .editor_type    = ED_SHIPMODULE,
    .cmd_table      = smedit_table,
    .show_fn        = smedit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_building,
    .perm           = {
        .flags          = OLC_PERM_CUSTOM,
        .check_fn       = smedit_check_perm
    },
    .change_mode    = OLC_CHANGE_CUSTOM,
    .mark_changed_fn = smedit_mark_changed,
    .audit_changes  = false,
    .get_history_fn = NULL,
};

/***************************************************************************
 * Entry Point                                                             *
 ***************************************************************************/

/**
 * do_smedit - Enter ship module template OLC editor
 *
 * Opens a module template for editing by VNUM. Creates new template
 * if 'create' is specified.
 *
 * Syntax: smedit <vnum>
 *         smedit create <vnum>
 *
 * @param ch        Staff member
 * @param argument  Module VNUM or 'create'
 */
void do_smedit(CHAR_DATA *ch, char *argument)
{
    SHIP_MODULE_INDEX *mod = NULL;
    char arg[MAX_STRING_LENGTH];
    WNUM wnum;

    if (IS_NPC(ch))
        return;

    if (!olc_editor_check_perm(ch, &smedit_def, NULL)) {
        send_to_char("SMEdit: Insufficient security to edit ship modules.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if (parse_widevnum(arg, ch->in_room->area, &wnum)) {
        if (!(mod = get_ship_module_index_for_area(wnum.pArea, wnum.vnum))) {
            send_to_char("That ship module does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &smedit_def, mod, false);
        return;
    }

    if (!str_cmp(arg, "create")) {
        if (smedit_create(ch, argument)) {
            olc_editor_enter(ch, &smedit_def, ch->desc->pEdit, false);
        }
        return;
    }

    send_to_char("Syntax: smedit <#vnum|area_uid#vnum>\n\r"
                 "        smedit create <vnum>\n\r", ch);
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * smedit - Command interpreter for the ship module template editor
 */
void smedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &smedit_def);
}

/***************************************************************************
 * Command Handlers                                                        *
 ***************************************************************************/

SMEDIT( smedit_list )
{
    AREA_DATA *area = ch->in_room->area;
    BUFFER *buffer = new_buf();
    int count = 0;

    add_buf(buffer, "{W[ Widevnum  ] Name                           Type        Size     Weight{x\n\r");
    add_buf(buffer, "=============================================================================\n\r");

    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (SHIP_MODULE_INDEX *mod = area->ship_module_index_hash[j]; mod; mod = mod->next) {
            if (mod->area != area) continue;
            char buf[MSL];
            sprintf(buf, "{G%10s  {Y%-30s {C%-10s {M%-8s {W%d{x\n\r",
                widevnum_string_ship_module(mod, NULL), mod->name,
                flag_string(hardpoint_types, mod->type),
                flag_string(hardpoint_sizes, mod->size),
                mod->weight);
            add_buf(buffer, buf);
            count++;
        }
    }

    if (count == 0)
        add_buf(buffer, "  No ship modules defined in this area.\n\r");

    add_buf(buffer, "=========================================================================\n\r");
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return false;
}

/**
 * smedit_show - Display current module template data
 *
 * Uses the unified OLC display framework for consistent formatting.
 *
 * @param ch        Character viewing
 * @param argument  Unused
 * @return          false (no data changed)
 */
SMEDIT( smedit_show )
{
    SHIP_MODULE_INDEX *mod;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&smedit_def);
    OLC_LAYOUT_CTX *ctx;

    EDIT_SHIPMODULE(ch, mod);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "SMEdit", mod->name,
        widevnum_string_ship_module(mod, NULL), &smedit_def);

    olc_display_string(ctx, theme, "Name:", "name", mod->name);
    olc_display_text(ctx, theme, "Description:", "desc", mod->description);

    olc_display_section(ctx, theme, "Slot Compatibility");
    olc_display_type(ctx, theme, "Type:", "type",
        hardpoint_types, mod->type);
    olc_display_type(ctx, theme, "Size:", "size",
        hardpoint_sizes, mod->size);
    olc_display_number(ctx, theme, "Weight:", "weight", mod->weight);
    olc_display_flags(ctx, theme, "Domain:", "domain",
        domain_flags, mod->domain_flags);
    olc_display_flags(ctx, theme, "Flags:", "flags",
        module_flags, mod->flags);

    olc_display_section(ctx, theme, "Stat Bonuses");
    olc_display_pair(ctx, theme,
        "Hit Bonus:", "hit", formatf("%d", mod->hit_bonus),
        "Armor Bonus:", "armor", formatf("%d", mod->armor_bonus));
    olc_display_pair(ctx, theme,
        "Speed Bonus:", "speed", formatf("%d%%", mod->speed_bonus),
        "Turning Bonus:", "turning", formatf("%d", mod->turning_bonus));
    olc_display_pair(ctx, theme,
        "Cargo Weight:", "cargo", formatf("%d", mod->cargo_weight_bonus),
        "Cargo Capacity:", "cargo", formatf("%d", mod->cargo_capacity_bonus));
    olc_display_number(ctx, theme, "Crew Bonus:", "crew", mod->crew_bonus);

    if (mod->type == HARDPOINT_WEAPON) {
        olc_display_section(ctx, theme, "Weapon Stats");
        olc_display_pair(ctx, theme,
            "Damage:", "damage", formatf("%d", mod->damage),
            "Range:", "range", formatf("%d", mod->range));
        olc_display_pair(ctx, theme,
            "Reload:", "reload", formatf("%d ticks", mod->reload_time),
            "Damage Type:", "damage", formatf("%d", mod->damage_type));
        olc_display_flags(ctx, theme, "Weapon Flags:", "weapflags",
            module_flags, mod->weapon_flags);

        if (mod->ammo) {
            olc_display_string(ctx, theme, "Ammo Object:", "ammo",
                formatf("%s (%s)", widevnum_string_object(mod->ammo, mod->area), mod->ammo->short_descr));
            olc_display_number(ctx, theme, "Ammo/Shot:", "ammo",
                mod->ammo_per_shot);
        } else {
            olc_display_string(ctx, theme, "Ammo:", "ammo", "Unlimited");
        }
    }

    olc_display_section(ctx, theme, "Crew Requirements");
    olc_display_number(ctx, theme, "Operators:", "operators", mod->operators);
    if (mod->req_gunning || mod->req_mechanics || mod->req_scouting ||
        mod->req_navigation || mod->req_oarring || mod->req_leadership) {
        if (mod->req_gunning)
            olc_display_infof(ctx, theme, "  {YGunning:    {W%d{x",
                mod->req_gunning);
        if (mod->req_mechanics)
            olc_display_infof(ctx, theme, "  {YMechanics:  {W%d{x",
                mod->req_mechanics);
        if (mod->req_scouting)
            olc_display_infof(ctx, theme, "  {YScouting:   {W%d{x",
                mod->req_scouting);
        if (mod->req_navigation)
            olc_display_infof(ctx, theme, "  {YNavigation: {W%d{x",
                mod->req_navigation);
        if (mod->req_oarring)
            olc_display_infof(ctx, theme, "  {YOarring:    {W%d{x",
                mod->req_oarring);
        if (mod->req_leadership)
            olc_display_infof(ctx, theme, "  {YLeadership: {W%d{x",
                mod->req_leadership);
    } else {
        olc_display_infof(ctx, theme, "  No skill requirements");
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

SMEDIT( smedit_create )
{
    SHIP_MODULE_INDEX *mod;
    AREA_DATA *pArea;
    long value;
    int iHash;

    /* Auto-vnum: empty or "0" finds next available */
    if (argument[0] == '\0' || !str_cmp(argument, "0")) {
        pArea = ch->in_room->area;
        value = 0;

        for (long try_vnum = 1; try_vnum < MAX_KEY_HASH * 100; try_vnum++) {
            if (!get_ship_module_index_for_area(pArea, try_vnum)) {
                value = try_vnum;
                break;
            }
        }

        if (value == 0) {
            send_to_char("SMEdit: Could not find an available vnum.\n\r", ch);
            return false;
        }
    } else {
        WNUM mod_wnum;
        if (!parse_widevnum(argument, ch->in_room->area, &mod_wnum)) {
            send_to_char("SMEdit: Invalid vnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        value = mod_wnum.vnum;
        pArea = mod_wnum.pArea;
    }

    if (!pArea) {
        send_to_char("SMEdit: That vnum is not assigned an area.\n\r", ch);
        return false;
    }

    if (!IS_BUILDER(ch, pArea)) {
        send_to_char("SMEdit: Vnum in an area you cannot build in.\n\r", ch);
        return false;
    }

    if (get_ship_module_index_for_area(pArea, value)) {
        send_to_char("SMEdit: That vnum already exists.\n\r", ch);
        return false;
    }

    mod = new_ship_module_index();
    mod->vnum = value;
    mod->area = pArea;

    iHash = mod->vnum % MAX_KEY_HASH;
    mod->next = pArea->ship_module_index_hash[iHash];
    pArea->ship_module_index_hash[iHash] = mod;
    ch->desc->pEdit = (void *)mod;

    send_to_char("Ship Module Created.\n\r", ch);
    SET_BIT(pArea->area_flags, AREA_CHANGED);
    return true;
}

/**
 * smedit_name - Set the module template name
 */
SMEDIT( smedit_name )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_string(ch, argument, "Name", NULL, &mod->name,
        OLC_STR_DEFAULT, NULL, NULL);
}

/**
 * smedit_desc - Edit the module description
 */
SMEDIT( smedit_desc )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &mod->description, NULL, NULL);
}

/**
 * smedit_type - Set the module hardpoint type (weapon/defense/utility/propulsion)
 */
SMEDIT( smedit_type )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_type_set(ch, argument, "Type",
        "Syntax: type <weapon|defense|utility|propulsion>",
        &mod->type, hardpoint_types, NULL, NULL);
}

/**
 * smedit_size - Set the module size (small/medium/large)
 */
SMEDIT( smedit_size )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_type_set(ch, argument, "Size",
        "Syntax: size <small|medium|large>",
        &mod->size, hardpoint_sizes, NULL, NULL);
}

/**
 * smedit_weight - Set the module weight (contributes to hull budget)
 */
SMEDIT( smedit_weight )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_number(ch, argument, "Weight", NULL,
        &mod->weight, 0, SHIP_MAX_MODULE_WEIGHT, NULL, NULL);
}

/**
 * smedit_domain - Toggle domain compatibility flags
 */
SMEDIT( smedit_domain )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);

    int value = flag_value(domain_flags, argument);
    if (value == NO_FLAG) {
        send_to_char("Syntax: domain <aquatic|aerial|terrestrial>\n\r", ch);
        return false;
    }

    mod->domain_flags ^= value;
    send_to_char(formatf("Domain flags: %s\n\r",
        flag_string(domain_flags, mod->domain_flags)), ch);
    return true;
}

/**
 * smedit_flags - Toggle module flags
 */
SMEDIT( smedit_flags )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);

    int value = flag_value(module_flags, argument);
    if (value == NO_FLAG) {
        send_to_char("Syntax: flags <flag>\n\r"
                     "Type '? module_flags' for a list.\n\r", ch);
        return false;
    }

    mod->flags ^= value;
    send_to_char(formatf("Module flags: %s\n\r",
        flag_string(module_flags, mod->flags)), ch);
    return true;
}

/**
 * smedit_hit - Set the hull HP bonus
 */
SMEDIT( smedit_hit )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_number(ch, argument, "Hit Bonus", NULL,
        &mod->hit_bonus, -10000, 10000, NULL, NULL);
}

/**
 * smedit_armor - Set the armor bonus
 */
SMEDIT( smedit_armor )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_number(ch, argument, "Armor Bonus", NULL,
        &mod->armor_bonus, -1000, 1000, NULL, NULL);
}

/**
 * smedit_speed - Set the speed bonus (percent)
 */
SMEDIT( smedit_speed )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_number(ch, argument, "Speed Bonus %", NULL,
        &mod->speed_bonus, -100, 200, NULL, NULL);
}

/**
 * smedit_turning - Set the turning bonus
 */
SMEDIT( smedit_turning )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_number(ch, argument, "Turning Bonus", NULL,
        &mod->turning_bonus, -60, 60, NULL, NULL);
}

/**
 * smedit_cargo - Set cargo bonuses (weight and capacity)
 *
 * Syntax: cargo weight <value>
 *         cargo capacity <value>
 */
SMEDIT( smedit_cargo )
{
    SHIP_MODULE_INDEX *mod;
    char arg[MIL];

    EDIT_SHIPMODULE(ch, mod);

    if (argument[0] == '\0') {
        send_to_char("Syntax: cargo weight <value>\n\r"
                     "        cargo capacity <value>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if (!str_cmp(arg, "weight")) {
        return olc_cmd_number(ch, argument, "Cargo Weight Bonus", NULL,
            &mod->cargo_weight_bonus, -50000, 50000, NULL, NULL);
    }

    if (!str_cmp(arg, "capacity")) {
        return olc_cmd_number(ch, argument, "Cargo Capacity Bonus", NULL,
            &mod->cargo_capacity_bonus, -5000, 5000, NULL, NULL);
    }

    send_to_char("Syntax: cargo weight <value>\n\r"
                 "        cargo capacity <value>\n\r", ch);
    return false;
}

/**
 * smedit_crew - Set crew capacity bonus
 */
SMEDIT( smedit_crew )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_number(ch, argument, "Crew Bonus", NULL,
        &mod->crew_bonus, -100, 100, NULL, NULL);
}

/**
 * smedit_damage - Set weapon damage and damage type
 *
 * Syntax: damage <value>
 *         damage type <value>
 */
SMEDIT( smedit_damage )
{
    SHIP_MODULE_INDEX *mod;
    char arg[MIL];

    EDIT_SHIPMODULE(ch, mod);

    if (argument[0] == '\0') {
        send_to_char("Syntax: damage <value>\n\r"
                     "        damage type <0-4>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if (!str_cmp(arg, "type")) {
        return olc_cmd_number(ch, argument, "Damage Type", NULL,
            &mod->damage_type, 0, 4, NULL, NULL);
    }

    /* Direct number = set base damage */
    if (is_number(arg)) {
        int val = atoi(arg);
        if (val < 0 || val > 10000) {
            send_to_char("Damage must be between 0 and 10000.\n\r", ch);
            return false;
        }
        mod->damage = val;
        send_to_char(formatf("Weapon damage set to %d.\n\r", val), ch);
        return true;
    }

    send_to_char("Syntax: damage <value>\n\r"
                 "        damage type <0-4>\n\r", ch);
    return false;
}

/**
 * smedit_range - Set weapon range in wilderness tiles
 */
SMEDIT( smedit_range )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_number(ch, argument, "Range", NULL,
        &mod->range, 0, 100, NULL, NULL);
}

/**
 * smedit_reload - Set weapon reload time in ticks
 */
SMEDIT( smedit_reload )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);
    return olc_cmd_number(ch, argument, "Reload Time", NULL,
        &mod->reload_time, 1, 100, NULL, NULL);
}

/**
 * smedit_weapflags - Toggle weapon-specific flags (AOE, anti-crew, etc.)
 */
SMEDIT( smedit_weapflags )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);

    int value = flag_value(module_flags, argument);
    if (value == NO_FLAG) {
        send_to_char("Syntax: weapflags <flag>\n\r"
                     "Type '? module_flags' for a list.\n\r", ch);
        return false;
    }

    mod->weapon_flags ^= value;
    send_to_char(formatf("Weapon flags: %s\n\r",
        flag_string(module_flags, mod->weapon_flags)), ch);
    return true;
}

/**
 * smedit_operators - Set how many crew members are needed to operate
 */
SMEDIT( smedit_operators )
{
    SHIP_MODULE_INDEX *mod;
    EDIT_SHIPMODULE(ch, mod);

    /* olc_cmd_number works on int, but operators is int16_t.
     * Use a temporary int and copy back. */
    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: operators <0-50>\n\r", ch);
        return false;
    }

    int val = atoi(argument);
    if (val < 0 || val > 50) {
        send_to_char("Operators must be between 0 and 50.\n\r", ch);
        return false;
    }

    mod->operators = (int16_t)val;
    send_to_char(formatf("Operators set to %d.\n\r", mod->operators), ch);
    return true;
}

/**
 * smedit_skills - Set crew skill requirements
 *
 * Syntax: skills <skill> <min_level>
 *         skills clear
 *
 * Skills: gunning, mechanics, scouting, navigation, oarring, leadership
 */
SMEDIT( smedit_skills )
{
    SHIP_MODULE_INDEX *mod;
    char arg[MIL];

    EDIT_SHIPMODULE(ch, mod);

    if (argument[0] == '\0') {
        send_to_char("Syntax: skills <skill> <min_level>\n\r"
                     "        skills clear\n\r"
                     "Skills: gunning, mechanics, scouting, navigation, oarring, leadership\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if (!str_cmp(arg, "clear")) {
        mod->req_gunning = 0;
        mod->req_mechanics = 0;
        mod->req_scouting = 0;
        mod->req_navigation = 0;
        mod->req_oarring = 0;
        mod->req_leadership = 0;
        send_to_char("All skill requirements cleared.\n\r", ch);
        return true;
    }

    if (!is_number(argument)) {
        send_to_char("Syntax: skills <skill> <min_level>\n\r", ch);
        return false;
    }

    int val = atoi(argument);
    if (val < 0 || val > 100) {
        send_to_char("Skill level must be between 0 and 100.\n\r", ch);
        return false;
    }

    int16_t level = (int16_t)val;

    if (!str_cmp(arg, "gunning"))         mod->req_gunning = level;
    else if (!str_cmp(arg, "mechanics"))  mod->req_mechanics = level;
    else if (!str_cmp(arg, "scouting"))   mod->req_scouting = level;
    else if (!str_cmp(arg, "navigation")) mod->req_navigation = level;
    else if (!str_cmp(arg, "oarring"))    mod->req_oarring = level;
    else if (!str_cmp(arg, "leadership")) mod->req_leadership = level;
    else {
        send_to_char("Unknown skill. Use: gunning, mechanics, scouting, navigation, oarring, leadership\n\r", ch);
        return false;
    }

    send_to_char(formatf("Skill requirement '%s' set to %d.\n\r", arg, level), ch);
    return true;
}

/**
 * smedit_ammo - Set ammo requirements
 *
 * Syntax: ammo <obj_vnum> <per_shot>
 *         ammo none
 */
SMEDIT( smedit_ammo )
{
    SHIP_MODULE_INDEX *mod;
    char arg[MIL];

    EDIT_SHIPMODULE(ch, mod);

    if (argument[0] == '\0') {
        send_to_char("Syntax: ammo <obj_vnum> <per_shot>\n\r"
                     "        ammo none\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if (!str_cmp(arg, "none")) {
        mod->ammo = NULL;
        mod->ammo_ref.load.auid = 0;
        mod->ammo_ref.load.vnum = 0;
        mod->ammo_per_shot = 0;
        REMOVE_BIT(mod->flags, MODULE_REQUIRES_AMMO);
        send_to_char("Ammo requirement removed.\n\r", ch);
        return true;
    }

    WNUM ammo_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(mod->area, arg);
    if (!parse_widevnum(arg, context, &ammo_wnum)) {
        send_to_char("Invalid vnum format.\n\r", ch);
        return false;
    }

    OBJ_INDEX_DATA *obj = get_obj_index(ammo_wnum.pArea, ammo_wnum.vnum);
    if (!obj) {
        send_to_char("That object does not exist.\n\r", ch);
        return false;
    }

    int per_shot = 1;
    if (is_number(argument))
        per_shot = atoi(argument);

    if (per_shot < 1 || per_shot > 100) {
        send_to_char("Ammo per shot must be between 1 and 100.\n\r", ch);
        return false;
    }

    mod->ammo = obj;
    mod->ammo_ref.load.auid = obj->area ? obj->area->uid : 0;
    mod->ammo_ref.load.vnum = obj->vnum;
    mod->ammo_per_shot = per_shot;
    SET_BIT(mod->flags, MODULE_REQUIRES_AMMO);

    send_to_char(formatf("Ammo set to '%s', %d per shot.\n\r",
        obj->short_descr, per_shot), ch);
    return true;
}

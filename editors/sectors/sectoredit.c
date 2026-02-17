#include <stdio.h>
#include <string.h>

#include <jansson.h>

#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../recycle.h"
#include "../../io/json/json_common.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

typedef struct sector_runtime_data SECTOR_RUNTIME_DATA;

struct sector_runtime_data {
    int id;
    char *name;
    int move_cost;
    int heal_rate;
    int mana_rate;
    char *comments;
};

SECTOREDIT(sectoredit_list);
SECTOREDIT(sectoredit_show);
SECTOREDIT(sectoredit_name);
SECTOREDIT(sectoredit_movecost);
SECTOREDIT(sectoredit_healrate);
SECTOREDIT(sectoredit_manarate);
SECTOREDIT(sectoredit_comments);
SECTOREDIT(sectoredit_save);
SECTOREDIT(sectoredit_reload);

#define SECTOREDIT_JSON_FILE SYSTEM_DIR "sectors.json"
#define SECTOREDIT_JSON_FORMAT "sectors"
#define SECTOREDIT_JSON_VERSION 1

static bool sectoredit_booted = false;
static SECTOR_RUNTIME_DATA sector_runtime[SECT_MAX];

extern int16_t movement_loss[SECT_MAX];

static int sectoredit_resolve_sector(char *argument);
static SECTOR_RUNTIME_DATA *sectoredit_current(CHAR_DATA *ch, int *out_index);
static bool sectoredit_save_to_json(void);
static bool sectoredit_load_from_json(void);
static void sectoredit_seed_defaults(void);

static const struct olc_cmd_type sectoredit_table[] = {
    { "?",         show_help },
    { "commands",  show_commands },
    { "list",      sectoredit_list },
    { "show",      sectoredit_show },
    { "name",      sectoredit_name },
    { "movecost",  sectoredit_movecost },
    { "healrate",  sectoredit_healrate },
    { "manarate",  sectoredit_manarate },
    { "comments",  sectoredit_comments },
    { "save",      sectoredit_save },
    { "reload",    sectoredit_reload },
    { NULL,         0 }
};

static const OLC_EDITOR_DEF sectoredit_def = {
    .name           = "SectorEdit",
    .editor_type    = ED_SECTOR,
    .cmd_table      = sectoredit_table,
    .show_fn        = sectoredit_show,
    .tabs           = {
        .count      = 1,
        .tabs       = {
            { "Basic", "Bas", NULL },
        },
    },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR,
    },
    .change_mode    = OLC_CHANGE_NONE,
    .audit_changes  = false,
};

static const char *sectoredit_default_name(int id)
{
    int i;

    for (i = 0; sector_flags[i].name != NULL; i++) {
        if (sector_flags[i].bit == id)
            return sector_flags[i].name;
    }

    return "unknown";
}

int sector_count(void)
{
    return SECT_MAX;
}

const char *sector_name(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return "unknown";

    return sector_runtime[index].name ? sector_runtime[index].name : "unknown";
}

int sector_move_cost(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return 1;

    return sector_runtime[index].move_cost;
}

int sector_lookup(const char *name)
{
    int i;

    load_sector_data();

    if (IS_NULLSTR(name))
        return NO_FLAG;

    for (i = 0; i < SECT_MAX; i++) {
        if (!str_prefix((char *)name, sector_runtime[i].name))
            return i;
    }

    return flag_value(sector_flags, (char *)name);
}

void load_sector_data(void)
{
    int i;

    if (sectoredit_booted)
        return;

    sectoredit_booted = true;
    sectoredit_seed_defaults();

    if (!sectoredit_load_from_json()) {
        if (!sectoredit_save_to_json())
            log_string("load_sector_data: Failed to seed sectors.json from defaults.");
    }

    for (i = 0; i < SECT_MAX; i++)
        movement_loss[i] = sector_runtime[i].move_cost;
}

static void sectoredit_seed_defaults(void)
{
    int i;

    for (i = 0; i < SECT_MAX; i++) {
        sector_runtime[i].id = i;

        if (sector_runtime[i].name)
            free_string(sector_runtime[i].name);
        if (sector_runtime[i].comments)
            free_string(sector_runtime[i].comments);

        sector_runtime[i].name = str_dup(sectoredit_default_name(i));
        sector_runtime[i].move_cost = movement_loss[i];
        sector_runtime[i].heal_rate = 100;
        sector_runtime[i].mana_rate = 100;
        sector_runtime[i].comments = str_dup("");
    }
}

static json_t *sectoredit_sector_to_json(const SECTOR_RUNTIME_DATA *sector)
{
    json_t *obj = json_object();

    json_object_set_new(obj, "id", json_integer(sector->id));
    json_object_set_new(obj, "name", json_string_safe(sector->name));
    json_object_set_new(obj, "move_cost", json_integer(sector->move_cost));
    json_object_set_new(obj, "heal_rate", json_integer(sector->heal_rate));
    json_object_set_new(obj, "mana_rate", json_integer(sector->mana_rate));
    json_object_set_new(obj, "comments", json_string_safe(sector->comments));

    return obj;
}

static bool sectoredit_save_to_json(void)
{
    json_t *root = json_object();
    json_t *array = json_array();
    int i;

    json_object_set_new(root, "_format", json_string(SECTOREDIT_JSON_FORMAT));
    json_object_set_new(root, "_version", json_integer(SECTOREDIT_JSON_VERSION));

    for (i = 0; i < SECT_MAX; i++)
        json_array_append_new(array, sectoredit_sector_to_json(&sector_runtime[i]));

    json_object_set_new(root, "sectors", array);

    return json_file_save(root, SECTOREDIT_JSON_FILE, "sectoredit_save_to_json",
        JSON_INDENT(2) | JSON_PRESERVE_ORDER);
}

static bool sectoredit_load_from_json(void)
{
    json_t *root;
    json_t *array = NULL;
    size_t index;
    json_t *entry;

    root = json_file_load(SECTOREDIT_JSON_FILE, "sectors", &array,
        "sectoredit_load_from_json");
    if (!root)
        return false;

    json_array_foreach(array, index, entry) {
        int id;
        const char *name;

        if (!json_is_object(entry))
            continue;

        id = (int)json_get_int(entry, "id", -1);
        if (id < 0 || id >= SECT_MAX)
            continue;

        name = json_get_string(entry, "name", sectoredit_default_name(id));

        free_string(sector_runtime[id].name);
        sector_runtime[id].name = str_dup(name);
        sector_runtime[id].move_cost = (int)json_get_int(entry, "move_cost", sector_runtime[id].move_cost);
        sector_runtime[id].heal_rate = (int)json_get_int(entry, "heal_rate", sector_runtime[id].heal_rate);
        sector_runtime[id].mana_rate = (int)json_get_int(entry, "mana_rate", sector_runtime[id].mana_rate);

        free_string(sector_runtime[id].comments);
        sector_runtime[id].comments = str_dup(json_get_string(entry, "comments", ""));
    }

    json_decref(root);
    return true;
}

void do_sectoredit(CHAR_DATA *ch, char *argument)
{
    int sector;

    if (IS_NPC(ch))
        return;

    load_sector_data();

    if (!olc_editor_check_perm(ch, &sectoredit_def, NULL)) {
        send_to_char("You don't have permission to edit sectors.\n\r", ch);
        return;
    }

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: sectoredit list\n\r", ch);
        send_to_char("        sectoredit <sector>\n\r", ch);
        return;
    }

    if (!str_cmp(argument, "list")) {
        sectoredit_list(ch, "");
        return;
    }

    sector = sectoredit_resolve_sector(argument);
    if (sector < 0 || sector >= SECT_MAX) {
        send_to_char("No sector found by that name or index.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &sectoredit_def, &sector_runtime[sector], true);
}

void sectoredit(CHAR_DATA *ch, char *argument)
{
    load_sector_data();
    olc_editor_interp(ch, argument, &sectoredit_def);
}

static int sectoredit_resolve_sector(char *argument)
{
    if (IS_NULLSTR(argument))
        return -1;

    if (is_number(argument))
        return atoi(argument);

    return sector_lookup(argument);
}

static SECTOR_RUNTIME_DATA *sectoredit_current(CHAR_DATA *ch, int *out_index)
{
    int i;

    if (!ch || !ch->desc || !ch->desc->pEdit)
        return NULL;

    for (i = 0; i < SECT_MAX; i++) {
        if (&sector_runtime[i] == (SECTOR_RUNTIME_DATA *)ch->desc->pEdit) {
            if (out_index)
                *out_index = i;
            return &sector_runtime[i];
        }
    }

    return NULL;
}

SECTOREDIT(sectoredit_list)
{
    int i;

    send_to_char("{WIdx  Name                 Move Heal Mana{X\n\r", ch);
    send_to_char("{D---- -------------------- ---- ---- ----{X\n\r", ch);

    for (i = 0; i < SECT_MAX; i++) {
        printf_to_char(ch, "{W%-4d %-20s %4d %4d %4d{X\n\r",
            i,
            sector_runtime[i].name,
            sector_runtime[i].move_cost,
            sector_runtime[i].heal_rate,
            sector_runtime[i].mana_rate);
    }

    return false;
}

SECTOREDIT(sectoredit_show)
{
    SECTOR_RUNTIME_DATA *sector;
    OLC_LAYOUT_CTX *ctx;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&sectoredit_def);
    int index = -1;

    sector = sectoredit_current(ch, &index);
    if (!sector)
        return false;

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "SectorEdit", sector->name, formatf("Sector %d", index), &sectoredit_def);

    olc_display_section(ctx, theme, "Basics");
    olc_display_string(ctx, theme, "Name:", "name", sector->name);
    olc_display_number(ctx, theme, "Move Cost:", "movecost", sector->move_cost);
    olc_display_number(ctx, theme, "Heal Rate:", "healrate", sector->heal_rate);
    olc_display_number(ctx, theme, "Mana Rate:", "manarate", sector->mana_rate);
    olc_display_text(ctx, theme, "Comments:", "comments", sector->comments);

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);

    return false;
}

SECTOREDIT(sectoredit_name)
{
    SECTOR_RUNTIME_DATA *sector = sectoredit_current(ch, NULL);
    bool changed;

    if (!sector)
        return false;

    changed = olc_cmd_string(ch, argument, "name", "name <text>",
        &sector->name, OLC_STR_DEFAULT, NULL, NULL);
    if (!changed)
        return false;

    if (!sectoredit_save_to_json())
        send_to_char("Name updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_movecost)
{
    SECTOR_RUNTIME_DATA *sector;
    int index = -1;
    bool changed;

    sector = sectoredit_current(ch, &index);
    if (!sector)
        return false;

    changed = olc_cmd_number(ch, argument, "movecost",
        "Syntax: movecost <1-200>\n\r",
        &sector->move_cost, 1, 200, NULL, NULL);
    if (!changed)
        return false;

    movement_loss[index] = sector->move_cost;

    if (!sectoredit_save_to_json())
        send_to_char("Move cost updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_healrate)
{
    SECTOR_RUNTIME_DATA *sector = sectoredit_current(ch, NULL);
    bool changed;

    if (!sector)
        return false;

    changed = olc_cmd_number(ch, argument, "healrate",
        "Syntax: healrate <1-1000>\n\r",
        &sector->heal_rate, 1, 1000, NULL, NULL);
    if (!changed)
        return false;

    if (!sectoredit_save_to_json())
        send_to_char("Heal rate updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_manarate)
{
    SECTOR_RUNTIME_DATA *sector = sectoredit_current(ch, NULL);
    bool changed;

    if (!sector)
        return false;

    changed = olc_cmd_number(ch, argument, "manarate",
        "Syntax: manarate <1-1000>\n\r",
        &sector->mana_rate, 1, 1000, NULL, NULL);
    if (!changed)
        return false;

    if (!sectoredit_save_to_json())
        send_to_char("Mana rate updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_comments)
{
    SECTOR_RUNTIME_DATA *sector = sectoredit_current(ch, NULL);
    bool changed;

    if (!sector)
        return false;

    changed = olc_cmd_string_append(ch, argument, "comments",
        "comments", &sector->comments, NULL, NULL);
    if (!changed)
        return false;

    if (!sectoredit_save_to_json())
        send_to_char("Comments updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_save)
{
    if (!sectoredit_save_to_json()) {
        send_to_char("Failed to save sectors.json.\n\r", ch);
        return false;
    }

    send_to_char("Saved sectors.json.\n\r", ch);
    return true;
}

SECTOREDIT(sectoredit_reload)
{
    sectoredit_seed_defaults();

    if (!sectoredit_load_from_json()) {
        send_to_char("Failed to load sectors.json (defaults restored).\n\r", ch);
        return false;
    }

    for (int i = 0; i < SECT_MAX; i++)
        movement_loss[i] = sector_runtime[i].move_cost;

    send_to_char("Reloaded sectors.json.\n\r", ch);
    return true;
}

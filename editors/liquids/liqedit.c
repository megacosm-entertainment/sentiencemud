#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

typedef struct liqedit_data LIQEDIT_DATA;

struct liqedit_data {
    long uid;
    char *name;
    char *color;
    char *comments;
    int16_t affect[LIQ_AFF_MAX];
    LIQEDIT_DATA *next;
};

LIQEDIT(liqedit_list);
LIQEDIT(liqedit_create);
LIQEDIT(liqedit_show);
LIQEDIT(liqedit_name);
LIQEDIT(liqedit_color);
LIQEDIT(liqedit_comments);
LIQEDIT(liqedit_proof);
LIQEDIT(liqedit_full);
LIQEDIT(liqedit_thirst);
LIQEDIT(liqedit_hunger);
LIQEDIT(liqedit_maxmana);
LIQEDIT(liqedit_fuel);
LIQEDIT(liqedit_vapor);
LIQEDIT(liqedit_delete);
LIQEDIT(liqedit_save);
LIQEDIT(liqedit_reload);

static LIQEDIT_DATA *liqedit_list_head = NULL;
static LIQEDIT_DATA *liqedit_list_tail = NULL;
static long liqedit_next_uid = 1;
static bool liqedit_booted = false;

static void liqedit_show_basic_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void liqedit_show_affects_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);

#define LIQEDIT_JSON_FILE SYSTEM_DIR "liquids.json"
#define LIQEDIT_JSON_FORMAT "liquids"
#define LIQEDIT_JSON_VERSION 1

typedef struct liqedit_default_data {
    const char *name;
    const char *color;
    int16_t affect[LIQ_AFF_MAX];
} LIQEDIT_DEFAULT_DATA;

static const LIQEDIT_DEFAULT_DATA liqedit_default_liquids[] = {
    { "water", "clear", { 0, 1, 10, 0, 16 } },
    { "beer", "amber", { 12, 1, 8, 1, 12 } },
    { "red wine", "burgundy", { 30, 1, 8, 1, 5 } },
    { "ale", "brown", { 15, 1, 8, 1, 12 } },
    { "dark ale", "dark", { 16, 1, 8, 1, 12 } },
    { "whisky", "golden", { 120, 1, 5, 0, 2 } },
    { "lemonade", "pink", { 0, 1, 9, 2, 12 } },
    { "firebreather", "boiling", { 190, 0, 4, 0, 2 } },
    { "local specialty", "clear", { 151, 1, 3, 0, 2 } },
    { "slime mold juice", "green", { 0, 2, -8, 1, 2 } },
    { "stew", "brown", { 0, 10, 10, 10, 12 } },
    { "milk", "white", { 0, 2, 9, 3, 12 } },
    { "tea", "tan", { 0, 1, 8, 0, 6 } },
    { "coffee", "black", { 0, 1, 8, 0, 6 } },
    { "blood", "red", { 0, 2, 0, 0, 6 } },
    { "salt water", "clear", { 0, 1, -2, 0, 1 } },
    { "coke", "brown", { 0, 2, 9, 2, 12 } },
    { "root beer", "brown", { 0, 2, 9, 2, 12 } },
    { "elvish wine", "green", { 35, 2, 8, 1, 5 } },
    { "white wine", "golden", { 28, 1, 8, 1, 5 } },
    { "champagne", "golden", { 32, 1, 8, 1, 5 } },
    { "mead", "honey-coloured", { 34, 2, 8, 2, 12 } },
    { "rose wine", "pink", { 26, 1, 8, 1, 5 } },
    { "benedictine wine", "burgundy", { 40, 1, 8, 1, 5 } },
    { "vodka", "clear", { 130, 1, 5, 0, 2 } },
    { "cranberry juice", "red", { 0, 1, 9, 2, 12 } },
    { "orange juice", "orange", { 0, 2, 9, 3, 12 } },
    { "absinthe", "green", { 200, 1, 4, 0, 2 } },
    { "brandy", "golden", { 80, 1, 5, 0, 4 } },
    { "aquavit", "clear", { 140, 1, 5, 0, 2 } },
    { "schnapps", "clear", { 90, 1, 5, 0, 2 } },
    { "ice wine", "purple", { 50, 2, 6, 1, 5 } },
    { "amontillado", "burgundy", { 35, 2, 8, 1, 5 } },
    { "sherry", "red", { 38, 2, 7, 1, 5 } },
    { "framboise", "red", { 50, 1, 7, 1, 5 } },
    { "rum", "amber", { 151, 1, 4, 0, 2 } },
    { "cordial", "clear", { 100, 1, 5, 0, 2 } },
    { "ammonia", "pale green", { 0, 1, 0, 0, 10 } },
    { "grog", "dark brown", { 40, 2, 4, 0, 12 } },
    { "snake oil", "viscous", { 0, 2, 1, 2, 10 } },
    { "vinegar", "pungent", { 0, 2, 1, 3, 8 } },
    { "acetone", "clear", { 0, 1, 0, 0, 9 } },
    { NULL, NULL, { 0, 0, 0, 0, 0 } }
};

static void liqedit_free_item(LIQEDIT_DATA *liq)
{
    if (!liq)
        return;

    free_string(liq->name);
    free_string(liq->color);
    free_string(liq->comments);
    free_mem(liq, sizeof(*liq));
}

static LIQEDIT_DATA *liqedit_get_by_index(int index)
{
    LIQEDIT_DATA *liq;
    int i = 0;

    if (index < 0)
        return NULL;

    for (liq = liqedit_list_head; liq; liq = liq->next, i++) {
        if (i == index)
            return liq;
    }

    return NULL;
}

static int liqedit_count(void)
{
    LIQEDIT_DATA *liq;
    int count = 0;

    for (liq = liqedit_list_head; liq; liq = liq->next)
        count++;

    return count;
}

static void liqedit_clear_all(void)
{
    LIQEDIT_DATA *liq = liqedit_list_head;
    LIQEDIT_DATA *next;

    while (liq) {
        next = liq->next;
        liqedit_free_item(liq);
        liq = next;
    }

    liqedit_list_head = NULL;
    liqedit_list_tail = NULL;
    liqedit_next_uid = 1;
}

static json_t *liqedit_item_to_json(const LIQEDIT_DATA *liq)
{
    json_t *obj = json_object();
    json_t *aff = json_array();
    int i;

    json_object_set_new(obj, "uid", json_integer(liq->uid));
    json_object_set_new(obj, "name", json_string_safe(liq->name));
    json_object_set_new(obj, "color", json_string_safe(liq->color));
    json_object_set_new(obj, "comments", json_string_safe(liq->comments));

    for (i = 0; i < LIQ_AFF_MAX; i++)
        json_array_append_new(aff, json_integer(liq->affect[i]));
    json_object_set_new(obj, "affects", aff);

    return obj;
}

static bool liqedit_load_from_json(void)
{
    json_t *root;
    json_t *liquids = NULL;
    size_t i;
    json_t *entry;
    long max_uid = 0;

    root = json_file_load(LIQEDIT_JSON_FILE, "liquids", &liquids,
        "liqedit_load_from_json");
    if (!root)
        return false;

    if (!json_is_array(liquids)) {
        json_decref(root);
        return false;
    }

    liqedit_clear_all();

    json_array_foreach(liquids, i, entry) {
        LIQEDIT_DATA *liq;
        json_t *affects;
        size_t ai;
        json_t *av;

        if (!json_is_object(entry))
            continue;

        liq = alloc_mem(sizeof(*liq));
        memset(liq, 0, sizeof(*liq));

        liq->uid = json_get_int(entry, "uid", 0);
        liq->name = str_dup(json_get_string(entry, "name", "liquid"));
        liq->color = str_dup(json_get_string(entry, "color", "clear"));
        liq->comments = str_dup(json_get_string(entry, "comments", ""));

        affects = json_object_get(entry, "affects");
        if (affects && json_is_array(affects)) {
            json_array_foreach(affects, ai, av) {
                if (ai >= LIQ_AFF_MAX)
                    break;
                if (json_is_integer(av))
                    liq->affect[ai] = (int16_t)json_integer_value(av);
            }
        }

        if (liq->uid <= 0)
            liq->uid = ++max_uid;
        if (liq->uid > max_uid)
            max_uid = liq->uid;

        if (!liqedit_list_head)
            liqedit_list_head = liq;
        else
            liqedit_list_tail->next = liq;
        liqedit_list_tail = liq;
    }

    liqedit_next_uid = UMAX(1, max_uid + 1);
    json_decref(root);
    return true;
}

static bool liqedit_save_to_json(void)
{
    json_t *root = json_object();
    json_t *liquids = json_array();
    LIQEDIT_DATA *liq;

    json_object_set_new(root, "_format", json_string(LIQEDIT_JSON_FORMAT));
    json_object_set_new(root, "_version", json_integer(LIQEDIT_JSON_VERSION));
    json_object_set_new(root, "next_uid", json_integer(liqedit_next_uid));

    for (liq = liqedit_list_head; liq; liq = liq->next)
        json_array_append_new(liquids, liqedit_item_to_json(liq));
    json_object_set_new(root, "liquids", liquids);

    return json_file_save(root, LIQEDIT_JSON_FILE, "liqedit_save_to_json",
        JSON_INDENT(2) | JSON_PRESERVE_ORDER);
}

static LIQEDIT_DATA *liqedit_find_uid(long uid)
{
    LIQEDIT_DATA *liq;

    for (liq = liqedit_list_head; liq; liq = liq->next)
        if (liq->uid == uid)
            return liq;

    return NULL;
}

static LIQEDIT_DATA *liqedit_find_name(const char *name)
{
    LIQEDIT_DATA *liq;

    if (IS_NULLSTR(name))
        return NULL;

    for (liq = liqedit_list_head; liq; liq = liq->next)
        if (!str_cmp(liq->name, name))
            return liq;

    return NULL;
}

static LIQEDIT_DATA *liqedit_new(const char *name, const char *color)
{
    LIQEDIT_DATA *liq = alloc_mem(sizeof(*liq));
    memset(liq, 0, sizeof(*liq));

    liq->uid = liqedit_next_uid++;
    liq->name = str_dup(name ? name : "liquid");
    liq->color = str_dup(color ? color : "clear");
    liq->comments = str_dup("");
    if (!liqedit_list_head)
        liqedit_list_head = liq;
    else
        liqedit_list_tail->next = liq;
    liqedit_list_tail = liq;

    return liq;
}

static bool liqedit_set_affect(CHAR_DATA *ch, LIQEDIT_DATA *liq, int index,
    char *argument, const char *label)
{
    bool changed;

    changed = olc_cmd_number_i16(ch, argument, label, NULL, &liq->affect[index],
        -32768, 32767, NULL, NULL);
    if (!changed)
        return false;

    if (!liqedit_save_to_json()) {
        send_to_char("Failed to save liquids.json after update.\n\r", ch);
        return true;
    }

    return true;
}

static void liqedit_copy_from_defaults(LIQEDIT_DATA *liq,
    const LIQEDIT_DEFAULT_DATA *defaults)
{
    int i;

    for (i = 0; i < LIQ_AFF_MAX; i++)
        liq->affect[i] = defaults->affect[i];
}

static void liqedit_seed_defaults(void)
{
    int i;

    liqedit_clear_all();

    for (i = 0; liqedit_default_liquids[i].name != NULL; i++) {
        const LIQEDIT_DEFAULT_DATA *defaults = &liqedit_default_liquids[i];
        LIQEDIT_DATA *liq = liqedit_new(defaults->name, defaults->color);
        liqedit_copy_from_defaults(liq, defaults);
    }
}

static void liqedit_ensure_loaded(void)
{
    if (liqedit_booted)
        return;

    liqedit_booted = true;

    if (!liqedit_load_from_json()) {
        liqedit_seed_defaults();
        liqedit_save_to_json();
    }
}

void load_liquid_data(void)
{
    liqedit_ensure_loaded();
}

int liquid_count(void)
{
    liqedit_ensure_loaded();

    return liqedit_count();
}

char *liquid_name(int index)
{
    LIQEDIT_DATA *liq;
    static char unknown_name[] = "unknown";

    liqedit_ensure_loaded();

    liq = liqedit_get_by_index(index);
    if (liq && !IS_NULLSTR(liq->name))
        return liq->name;

    return unknown_name;
}

char *liquid_color(int index)
{
    LIQEDIT_DATA *liq;
    static char clear_color[] = "clear";

    liqedit_ensure_loaded();

    liq = liqedit_get_by_index(index);
    if (liq && !IS_NULLSTR(liq->color))
        return liq->color;

    return clear_color;
}

int liquid_affect(int index, int affect_index)
{
    LIQEDIT_DATA *liq;

    if (affect_index < 0 || affect_index >= LIQ_AFF_MAX)
        return 0;

    liqedit_ensure_loaded();

    liq = liqedit_get_by_index(index);
    if (liq)
        return liq->affect[affect_index];

    return 0;
}

int liquid_lookup(const char *name)
{
    LIQEDIT_DATA *liq;
    int index = 0;

    if (IS_NULLSTR(name))
        return -1;

    liqedit_ensure_loaded();

    for (liq = liqedit_list_head; liq; liq = liq->next, index++) {
        if (LOWER(name[0]) == LOWER(liq->name[0])
            && !str_prefix(name, liq->name))
            return index;
    }

    return -1;
}

long liquid_uid(int index)
{
    LIQEDIT_DATA *liq;

    liqedit_ensure_loaded();

    liq = liqedit_get_by_index(index);
    if (liq)
        return liq->uid;

    return 0;
}

int liquid_index_from_uid(long uid)
{
    LIQEDIT_DATA *liq;
    int index = 0;

    if (uid <= 0)
        return -1;

    liqedit_ensure_loaded();

    for (liq = liqedit_list_head; liq; liq = liq->next, index++) {
        if (liq->uid == uid)
            return index;
    }

    return -1;
}

const struct olc_cmd_type liqedit_table[] = {
    { "?",        show_help       },
    { "commands", show_commands   },
    { "create",   liqedit_create  },
    { "delete",   liqedit_delete  },
    { "list",     liqedit_list    },
    { "show",     liqedit_show    },
    { "name",     liqedit_name    },
    { "reload",   liqedit_reload  },
    { "color",    liqedit_color   },
    { "comments", liqedit_comments},
    { "proof",    liqedit_proof   },
    { "full",     liqedit_full    },
    { "thirst",   liqedit_thirst  },
    { "hunger",   liqedit_hunger  },
    { "maxmana",  liqedit_maxmana },
    { "fuel",     liqedit_fuel    },
    { "vapor",    liqedit_vapor   },
    { "save",     liqedit_save    },
    { NULL,         0               }
};

static const OLC_EDITOR_DEF liqedit_def = {
    .name           = "LiqEdit",
    .editor_type    = ED_LIQUID,
    .cmd_table      = liqedit_table,
    .show_fn        = liqedit_show,
    .tabs           = {
        .count      = 2,
        .tabs       = {
            { "Basic",   "Bas", liqedit_show_basic_tab },
            { "Affects", "Aff", liqedit_show_affects_tab },
        },
    },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR,
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = true,
};

void do_liqedit(CHAR_DATA *ch, char *argument)
{
    LIQEDIT_DATA *liq;
    char arg1[MIL];

    if (IS_NPC(ch))
        return;

    liqedit_ensure_loaded();

    if (!olc_editor_check_perm(ch, &liqedit_def, NULL)) {
        send_to_char("You don't have permission to edit liquids.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax: liqedit list\n\r", ch);
        send_to_char("        liqedit create <name>\n\r", ch);
        send_to_char("        liqedit reload\n\r", ch);
        send_to_char("        liqedit <uid|name>\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "list")) {
        liqedit_list(ch, argument);
        return;
    }

    if (!str_cmp(arg1, "create")) {
        liqedit_create(ch, argument);
        return;
    }

    if (!str_cmp(arg1, "reload")) {
        liqedit_reload(ch, argument);
        return;
    }

    if (is_number(arg1))
        liq = liqedit_find_uid(atol(arg1));
    else
        liq = liqedit_find_name(arg1);

    if (!liq) {
        send_to_char("No liquid found by that uid or name.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &liqedit_def, liq, true);
}

void liqedit(CHAR_DATA *ch, char *argument)
{
    liqedit_ensure_loaded();
    olc_editor_interp(ch, argument, &liqedit_def);
}

LIQEDIT(liqedit_list)
{
    LIQEDIT_DATA *liq;

    if (!liqedit_list_head) {
        send_to_char("No liquids defined.\n\r", ch);
        return false;
    }

    send_to_char("{WUID   Name                     Color{X\n\r", ch);
    send_to_char("{D----- ------------------------ ----------------{X\n\r", ch);
    for (liq = liqedit_list_head; liq; liq = liq->next)
        printf_to_char(ch, "{W%-5ld {x%-24s %-16s{X\n\r",
            liq->uid, liq->name, liq->color);

    return false;
}

LIQEDIT(liqedit_create)
{
    LIQEDIT_DATA *liq;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: create <name>\n\r", ch);
        return false;
    }

    if (liqedit_find_name(argument)) {
        send_to_char("A liquid with that name already exists.\n\r", ch);
        return false;
    }

    liq = liqedit_new(argument, "clear");

    if (!liqedit_save_to_json()) {
        send_to_char("Liquid created, but failed to save liquids.json.\n\r", ch);
        return true;
    }

    send_to_char("Liquid created.\n\r", ch);
    olc_editor_enter(ch, &liqedit_def, liq, true);
    return true;
}

LIQEDIT(liqedit_show)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&liqedit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    if (!liq)
        return false;

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "LiqEdit", liq->name,
        formatf("UID %ld", liq->uid), &liqedit_def);

    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        for (int i = 0; i < liqedit_def.tabs.count; i++) {
            if (liqedit_def.tabs.tabs[i].show_fn)
                liqedit_def.tabs.tabs[i].show_fn(ch, ctx, (void *)liq);
        }
    } else if (tab >= 0 && tab < liqedit_def.tabs.count
        && liqedit_def.tabs.tabs[tab].show_fn) {
        liqedit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)liq);
    } else {
        liqedit_show_basic_tab(ch, ctx, (void *)liq);
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

static void liqedit_show_basic_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&liqedit_def);

    (void)ch;

    olc_display_string(ctx, theme, "Name:", "name", liq->name);
    olc_display_string(ctx, theme, "Color:", "color", liq->color);
    olc_display_text(ctx, theme, "Comments:", "comments",
        IS_NULLSTR(liq->comments) ? NULL : liq->comments);
}

static void liqedit_show_affects_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&liqedit_def);

    (void)ch;

    olc_display_section(ctx, theme, "Core Liquid Affects");
    olc_display_number(ctx, theme, "Proof:", "proof", liq->affect[LIQ_AFF_PROOF]);
    olc_display_number(ctx, theme, "Full:", "full", liq->affect[LIQ_AFF_FULL]);
    olc_display_number(ctx, theme, "Thirst:", "thirst", liq->affect[LIQ_AFF_THIRST]);
    olc_display_number(ctx, theme, "Hunger:", "hunger", liq->affect[LIQ_AFF_HUNGER]);
    olc_display_number(ctx, theme, "MaxMana:", "maxmana", liq->affect[LIQ_AFF_SSIZE]);
    olc_display_number(ctx, theme, "Fuel:", "fuel", liq->affect[LIQ_AFF_FUEL]);
    olc_display_number(ctx, theme, "Vapor:", "vapor", liq->affect[LIQ_AFF_VAPOR]);

    olc_display_blank(ctx);
    olc_display_section(ctx, theme, "Buff Extensions (Stub)");
    olc_display_infof(ctx, theme,
        "%sPlanned:%s satiety/food/drink buff profiles will be attached here in a later phase.",
        theme->label, theme->value);
    olc_display_infof(ctx, theme,
        "%sStub:%s this tab is reserved for per-liquid buff definitions and effect lists.",
        theme->label, theme->value);
}

LIQEDIT(liqedit_name)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    bool changed;

    if (!liq)
        return false;

    if (!IS_NULLSTR(argument)
        && liqedit_find_name(argument)
        && str_cmp(liq->name, argument)) {
        send_to_char("A liquid with that name already exists.\n\r", ch);
        return false;
    }

    changed = olc_cmd_string(ch, argument, "name", "name <new name>",
        &liq->name, OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, NULL, NULL);
    if (!changed)
        return false;

    if (!liqedit_save_to_json()) {
        send_to_char("Name updated, but failed to save liquids.json.\n\r", ch);
        return true;
    }

    return true;
}

LIQEDIT(liqedit_color)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    bool changed;

    if (!liq)
        return false;

    changed = olc_cmd_string(ch, argument, "color", "color <new color>",
        &liq->color, OLC_STR_DEFAULT, NULL, NULL);
    if (!changed)
        return false;

    if (!liqedit_save_to_json()) {
        send_to_char("Color updated, but failed to save liquids.json.\n\r", ch);
        return true;
    }

    return true;
}

LIQEDIT(liqedit_comments)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    bool changed;

    if (!liq)
        return false;

    changed = olc_cmd_string(ch, argument, "comments", "comments <text>",
        &liq->comments, OLC_STR_DEFAULT, NULL, NULL);
    if (!changed)
        return false;

    if (!liqedit_save_to_json()) {
        send_to_char("Comments updated, but failed to save liquids.json.\n\r", ch);
        return true;
    }

    return true;
}

LIQEDIT(liqedit_proof)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    return liq ? liqedit_set_affect(ch, liq, LIQ_AFF_PROOF, argument, "proof") : false;
}

LIQEDIT(liqedit_full)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    return liq ? liqedit_set_affect(ch, liq, LIQ_AFF_FULL, argument, "full") : false;
}

LIQEDIT(liqedit_thirst)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    return liq ? liqedit_set_affect(ch, liq, LIQ_AFF_THIRST, argument, "thirst") : false;
}

LIQEDIT(liqedit_hunger)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    return liq ? liqedit_set_affect(ch, liq, LIQ_AFF_HUNGER, argument, "hunger") : false;
}

LIQEDIT(liqedit_maxmana)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    return liq ? liqedit_set_affect(ch, liq, LIQ_AFF_SSIZE, argument, "maxmana") : false;
}

LIQEDIT(liqedit_fuel)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    return liq ? liqedit_set_affect(ch, liq, LIQ_AFF_FUEL, argument, "fuel") : false;
}

LIQEDIT(liqedit_vapor)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    return liq ? liqedit_set_affect(ch, liq, LIQ_AFF_VAPOR, argument, "vapor") : false;
}

LIQEDIT(liqedit_delete)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    LIQEDIT_DATA *it;
    LIQEDIT_DATA *prev = NULL;

    if (!liq)
        return false;

    for (it = liqedit_list_head; it; prev = it, it = it->next) {
        if (it != liq)
            continue;

        if (prev)
            prev->next = it->next;
        else
            liqedit_list_head = it->next;

        if (liqedit_list_tail == it)
            liqedit_list_tail = prev;

        liqedit_free_item(it);

        if (!liqedit_save_to_json())
            send_to_char("Liquid deleted, but failed to save liquids.json.\n\r", ch);
        else
            send_to_char("Liquid deleted.\n\r", ch);
        edit_done(ch);
        return true;
    }

    return false;
}

LIQEDIT(liqedit_save)
{
    liqedit_ensure_loaded();

    if (!liqedit_save_to_json()) {
        send_to_char("Failed to save liquids.json.\n\r", ch);
        return false;
    }

    send_to_char("Liquid data saved to liquids.json.\n\r", ch);
    return false;
}

LIQEDIT(liqedit_reload)
{
    LIQEDIT_DATA *liq = (LIQEDIT_DATA *)ch->desc->pEdit;
    LIQEDIT_DATA *reloaded = NULL;
    long uid = 0;

    liqedit_ensure_loaded();

    if (liq)
        uid = liq->uid;

    if (!liqedit_load_from_json()) {
        send_to_char("Failed to reload liquids.json.\n\r", ch);
        return false;
    }

    if (uid > 0)
        reloaded = liqedit_find_uid(uid);

    if (uid > 0 && !reloaded) {
        send_to_char("Liquids reloaded, but current liquid no longer exists.\n\r", ch);
        edit_done(ch);
        return false;
    }

    if (reloaded && ch->desc)
        ch->desc->pEdit = reloaded;

    send_to_char("Liquids reloaded from disk.\n\r", ch);
    return false;
}
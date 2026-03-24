#include <stdio.h>
#include <stdlib.h>
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

extern const struct material_type material_table[];

typedef struct matedit_data MATEDIT_DATA;

struct matedit_data {
    long uid;
    char *name;
    int16_t strength;
    int16_t value;
    char *comments;
    MATEDIT_DATA *next;
};

MATEDIT(matedit_list);
MATEDIT(matedit_create);
MATEDIT(matedit_show);
MATEDIT(matedit_name);
MATEDIT(matedit_strength);
MATEDIT(matedit_value);
MATEDIT(matedit_comments);
MATEDIT(matedit_delete);
MATEDIT(matedit_save);
MATEDIT(matedit_reload);

static MATEDIT_DATA *matedit_list_head = NULL;
static MATEDIT_DATA *matedit_list_tail = NULL;
static long matedit_next_uid = 1;
static bool matedit_booted = false;

#define MATEDIT_JSON_FILE SYSTEM_DIR "materials.json"
#define MATEDIT_JSON_FORMAT "materials"
#define MATEDIT_JSON_VERSION 1

static void matedit_show_basic_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);

static void matedit_free_item(MATEDIT_DATA *mat)
{
    if (!mat)
        return;

    free_string(mat->name);
    free_string(mat->comments);
    free_mem(mat, sizeof(*mat));
}

static void matedit_clear_all(void)
{
    MATEDIT_DATA *mat = matedit_list_head;
    MATEDIT_DATA *next;

    while (mat) {
        next = mat->next;
        matedit_free_item(mat);
        mat = next;
    }

    matedit_list_head = NULL;
    matedit_list_tail = NULL;
    matedit_next_uid = 1;
}

static int matedit_count(void)
{
    MATEDIT_DATA *mat;
    int count = 0;

    for (mat = matedit_list_head; mat; mat = mat->next)
        count++;

    return count;
}

static MATEDIT_DATA *matedit_new(const char *name)
{
    MATEDIT_DATA *mat = alloc_mem(sizeof(*mat));
    memset(mat, 0, sizeof(*mat));

    mat->uid = matedit_next_uid++;
    mat->name = str_dup(name ? name : "material");
    mat->strength = 1;
    mat->value = 1;
    mat->comments = str_dup("");

    if (!matedit_list_head)
        matedit_list_head = mat;
    else
        matedit_list_tail->next = mat;
    matedit_list_tail = mat;

    return mat;
}

static MATEDIT_DATA *matedit_find_uid(long uid)
{
    MATEDIT_DATA *mat;

    for (mat = matedit_list_head; mat; mat = mat->next)
        if (mat->uid == uid)
            return mat;

    return NULL;
}

static MATEDIT_DATA *matedit_find_name(const char *name)
{
    MATEDIT_DATA *mat;

    if (IS_NULLSTR(name))
        return NULL;

    for (mat = matedit_list_head; mat; mat = mat->next)
        if (!str_cmp(mat->name, name))
            return mat;

    return NULL;
}

static MATEDIT_DATA *matedit_get_by_index(int index)
{
    MATEDIT_DATA *mat;
    int i = 0;

    if (index < 0)
        return NULL;

    for (mat = matedit_list_head; mat; mat = mat->next, i++)
        if (i == index)
            return mat;

    return NULL;
}

static json_t *matedit_item_to_json(const MATEDIT_DATA *mat)
{
    json_t *obj = json_object();

    json_object_set_new(obj, "uid", json_integer(mat->uid));
    json_object_set_new(obj, "name", json_string_safe(mat->name));
    json_object_set_new(obj, "strength", json_integer(mat->strength));
    json_object_set_new(obj, "value", json_integer(mat->value));
    json_object_set_new(obj, "comments", json_string_safe(mat->comments));

    return obj;
}

static bool matedit_save_to_json(void)
{
    json_t *root = json_object();
    json_t *materials = json_array();
    MATEDIT_DATA *mat;

    json_object_set_new(root, "_format", json_string(MATEDIT_JSON_FORMAT));
    json_object_set_new(root, "_version", json_integer(MATEDIT_JSON_VERSION));
    json_object_set_new(root, "next_uid", json_integer(matedit_next_uid));

    for (mat = matedit_list_head; mat; mat = mat->next)
        json_array_append_new(materials, matedit_item_to_json(mat));
    json_object_set_new(root, "materials", materials);

    return json_file_save(root, MATEDIT_JSON_FILE, "matedit_save_to_json",
        JSON_INDENT(2) | JSON_PRESERVE_ORDER);
}

static bool matedit_load_from_json(void)
{
    json_t *root;
    json_t *materials = NULL;
    size_t i;
    json_t *entry;
    long max_uid = 0;

    root = json_file_load(MATEDIT_JSON_FILE, "materials", &materials,
        "matedit_load_from_json");
    if (!root)
        return false;

    if (!json_is_array(materials)) {
        json_decref(root);
        return false;
    }

    matedit_clear_all();

    json_array_foreach(materials, i, entry) {
        MATEDIT_DATA *mat;

        if (!json_is_object(entry))
            continue;

        mat = alloc_mem(sizeof(*mat));
        memset(mat, 0, sizeof(*mat));

        mat->uid = json_get_int(entry, "uid", 0);
        mat->name = str_dup(json_get_string(entry, "name", "material"));
        mat->strength = (int16_t)json_get_int(entry, "strength", 1);
        mat->value = (int16_t)json_get_int(entry, "value", 1);
        mat->comments = str_dup(json_get_string(entry, "comments", ""));

        if (mat->uid <= 0)
            mat->uid = ++max_uid;
        if (mat->uid > max_uid)
            max_uid = mat->uid;

        if (!matedit_list_head)
            matedit_list_head = mat;
        else
            matedit_list_tail->next = mat;
        matedit_list_tail = mat;
    }

    matedit_next_uid = UMAX(1, max_uid + 1);
    json_decref(root);
    return true;
}

static void matedit_seed_from_legacy(void)
{
    int i;

    matedit_clear_all();

    for (i = 0; material_table[i].name != NULL; i++) {
        MATEDIT_DATA *mat = matedit_new(material_table[i].name);
        mat->strength = (int16_t)material_table[i].strength;
        mat->value = (int16_t)material_table[i].value;
    }
}

static bool matedit_merge_missing_from_legacy(void)
{
    int i;
    bool changed = false;

    for (i = 0; material_table[i].name != NULL; i++) {
        MATEDIT_DATA *mat = matedit_find_name(material_table[i].name);

        if (mat != NULL)
            continue;

        mat = matedit_new(material_table[i].name);
        mat->strength = (int16_t)material_table[i].strength;
        mat->value = (int16_t)material_table[i].value;
        changed = true;
    }

    return changed;
}

static void matedit_ensure_loaded(void)
{
    bool changed = false;

    if (matedit_booted)
        return;

    matedit_booted = true;

    if (!matedit_load_from_json()) {
        matedit_seed_from_legacy();
        changed = true;
    } else {
        changed = matedit_merge_missing_from_legacy();
    }

    if (changed)
        matedit_save_to_json();
}

void load_material_data(void)
{
    matedit_ensure_loaded();
}

int material_count(void)
{
    matedit_ensure_loaded();
    return matedit_count();
}

char *material_name(int index)
{
    MATEDIT_DATA *mat;
    static char unknown[] = "unknown";

    matedit_ensure_loaded();

    mat = matedit_get_by_index(index);
    if (mat && !IS_NULLSTR(mat->name))
        return mat->name;

    return unknown;
}

int material_strength(int index)
{
    MATEDIT_DATA *mat;

    matedit_ensure_loaded();

    mat = matedit_get_by_index(index);
    if (mat)
        return mat->strength;

    return 1;
}

int material_value(int index)
{
    MATEDIT_DATA *mat;

    matedit_ensure_loaded();

    mat = matedit_get_by_index(index);
    if (mat)
        return mat->value;

    return 1;
}

int material_index_lookup(const char *name)
{
    MATEDIT_DATA *mat;
    int index = 0;

    if (IS_NULLSTR(name))
        return -1;

    matedit_ensure_loaded();

    for (mat = matedit_list_head; mat; mat = mat->next, index++) {
        if (LOWER(name[0]) == LOWER(mat->name[0]) && !str_prefix(name, mat->name))
            return index;
    }

    return -1;
}

const struct olc_cmd_type matedit_table[] = {
    { "?",        show_help       },
    { "commands", show_commands   },
    { "create",   matedit_create  },
    { "delete",   matedit_delete  },
    { "list",     matedit_list    },
    { "show",     matedit_show    },
    { "name",     matedit_name    },
    { "strength", matedit_strength},
    { "value",    matedit_value   },
    { "comments", matedit_comments},
    { "save",     matedit_save    },
    { "reload",   matedit_reload  },
    { NULL,         0               }
};

static const OLC_EDITOR_DEF matedit_def = {
    .name           = "MatEdit",
    .editor_type    = ED_MATERIAL,
    .cmd_table      = matedit_table,
    .show_fn        = matedit_show,
    .tabs           = {
        .count      = 1,
        .tabs       = {
            { "Basic", "Bas", matedit_show_basic_tab },
        },
    },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR,
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = false,
};

void do_matedit(CHAR_DATA *ch, char *argument)
{
    MATEDIT_DATA *mat;
    char arg1[MIL];

    if (IS_NPC(ch))
        return;

    matedit_ensure_loaded();

    if (!olc_editor_check_perm(ch, &matedit_def, NULL)) {
        send_to_char("You don't have permission to edit materials.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax: matedit list\n\r", ch);
        send_to_char("        matedit create <name>\n\r", ch);
        send_to_char("        matedit reload\n\r", ch);
        send_to_char("        matedit <uid|name>\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "list")) {
        matedit_list(ch, argument);
        return;
    }

    if (!str_cmp(arg1, "create")) {
        matedit_create(ch, argument);
        return;
    }

    if (!str_cmp(arg1, "reload")) {
        matedit_reload(ch, argument);
        return;
    }

    if (is_number(arg1))
        mat = matedit_find_uid(atol(arg1));
    else
        mat = matedit_find_name(arg1);

    if (!mat) {
        send_to_char("No material found by that uid or name.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &matedit_def, mat, true);
}

void matedit(CHAR_DATA *ch, char *argument)
{
    matedit_ensure_loaded();
    olc_editor_interp(ch, argument, &matedit_def);
}

MATEDIT(matedit_list)
{
    MATEDIT_DATA *mat;

    if (!matedit_list_head) {
        send_to_char("No materials defined.\n\r", ch);
        return false;
    }

    send_to_char("{WUID   Name                     Strength Value{X\n\r", ch);
    send_to_char("{D----- ------------------------ -------- -----{X\n\r", ch);
    for (mat = matedit_list_head; mat; mat = mat->next) {
        printf_to_char(ch, "{W%-5ld {x%-24s %8d %5d{X\n\r",
            mat->uid, mat->name, mat->strength, mat->value);
    }

    return false;
}

MATEDIT(matedit_create)
{
    MATEDIT_DATA *mat;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: create <name>\n\r", ch);
        return false;
    }

    if (matedit_find_name(argument)) {
        send_to_char("A material with that name already exists.\n\r", ch);
        return false;
    }

    mat = matedit_new(argument);

    if (!matedit_save_to_json()) {
        send_to_char("Material created, but failed to save materials.json.\n\r", ch);
        return true;
    }

    send_to_char("Material created.\n\r", ch);
    olc_editor_enter(ch, &matedit_def, mat, true);
    return true;
}

MATEDIT(matedit_show)
{
    MATEDIT_DATA *mat = (MATEDIT_DATA *)ch->desc->pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&matedit_def);
    OLC_LAYOUT_CTX *ctx;

    if (!mat)
        return false;

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "MatEdit", mat->name,
        formatf("UID %ld", mat->uid), &matedit_def);
    matedit_show_basic_tab(ch, ctx, (void *)mat);
    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

static void matedit_show_basic_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    MATEDIT_DATA *mat = (MATEDIT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&matedit_def);

    (void)ch;

    olc_display_string(ctx, theme, "Name:", "name", mat->name);
    olc_display_number(ctx, theme, "Strength:", "strength", mat->strength);
    olc_display_number(ctx, theme, "Value:", "value", mat->value);
    olc_display_text(ctx, theme, "Comments:", "comments",
        IS_NULLSTR(mat->comments) ? NULL : mat->comments);
}

MATEDIT(matedit_name)
{
    MATEDIT_DATA *mat = (MATEDIT_DATA *)ch->desc->pEdit;
    bool changed;

    if (!mat)
        return false;

    if (!IS_NULLSTR(argument)
        && matedit_find_name(argument)
        && str_cmp(mat->name, argument)) {
        send_to_char("A material with that name already exists.\n\r", ch);
        return false;
    }

    changed = olc_cmd_string(ch, argument, "name", "name <new name>",
        &mat->name, OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, NULL, NULL);
    if (!changed)
        return false;

    if (!matedit_save_to_json()) {
        send_to_char("Name updated, but failed to save materials.json.\n\r", ch);
        return true;
    }

    return true;
}

MATEDIT(matedit_strength)
{
    MATEDIT_DATA *mat = (MATEDIT_DATA *)ch->desc->pEdit;
    bool changed;

    if (!mat)
        return false;

    changed = olc_cmd_number_i16(ch, argument, "strength",
        "Syntax: strength <positive number>\n\r",
        &mat->strength, 1, 32767, NULL, NULL);
    if (!changed)
        return false;

    if (!matedit_save_to_json()) {
        send_to_char("Strength updated, but failed to save materials.json.\n\r", ch);
        return true;
    }

    return true;
}

MATEDIT(matedit_value)
{
    MATEDIT_DATA *mat = (MATEDIT_DATA *)ch->desc->pEdit;
    bool changed;

    if (!mat)
        return false;

    changed = olc_cmd_number_i16(ch, argument, "value",
        "Syntax: value <non-negative number>\n\r",
        &mat->value, 0, 32767, NULL, NULL);
    if (!changed)
        return false;

    if (!matedit_save_to_json()) {
        send_to_char("Value updated, but failed to save materials.json.\n\r", ch);
        return true;
    }

    return true;
}

MATEDIT(matedit_comments)
{
    MATEDIT_DATA *mat = (MATEDIT_DATA *)ch->desc->pEdit;
    bool changed;

    if (!mat)
        return false;

    changed = olc_cmd_string(ch, argument, "comments", "comments <text>",
        &mat->comments, OLC_STR_DEFAULT, NULL, NULL);
    if (!changed)
        return false;

    if (!matedit_save_to_json()) {
        send_to_char("Comments updated, but failed to save materials.json.\n\r", ch);
        return true;
    }

    return true;
}

MATEDIT(matedit_delete)
{
    MATEDIT_DATA *mat = (MATEDIT_DATA *)ch->desc->pEdit;
    MATEDIT_DATA *it;
    MATEDIT_DATA *prev = NULL;

    if (!mat)
        return false;

    for (it = matedit_list_head; it; prev = it, it = it->next) {
        if (it != mat)
            continue;

        if (prev)
            prev->next = it->next;
        else
            matedit_list_head = it->next;

        if (matedit_list_tail == it)
            matedit_list_tail = prev;

        matedit_free_item(it);

        if (!matedit_save_to_json())
            send_to_char("Material deleted, but failed to save materials.json.\n\r", ch);
        else
            send_to_char("Material deleted.\n\r", ch);

        edit_done(ch);
        return true;
    }

    return false;
}

MATEDIT(matedit_save)
{
    matedit_ensure_loaded();

    if (!matedit_save_to_json()) {
        send_to_char("Failed to save materials.json.\n\r", ch);
        return false;
    }

    send_to_char("Material data saved to materials.json.\n\r", ch);
    return false;
}

MATEDIT(matedit_reload)
{
    MATEDIT_DATA *mat = (MATEDIT_DATA *)ch->desc->pEdit;
    MATEDIT_DATA *reloaded = NULL;
    long uid = 0;

    matedit_ensure_loaded();

    if (mat)
        uid = mat->uid;

    if (!matedit_load_from_json()) {
        send_to_char("Failed to reload materials.json.\n\r", ch);
        return false;
    }

    if (uid > 0)
        reloaded = matedit_find_uid(uid);

    if (uid > 0 && !reloaded) {
        send_to_char("Materials reloaded, but current material no longer exists.\n\r", ch);
        edit_done(ch);
        return false;
    }

    if (reloaded && ch->desc)
        ch->desc->pEdit = reloaded;

    send_to_char("Materials reloaded from disk.\n\r", ch);
    return false;
}
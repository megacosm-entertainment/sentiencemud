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
#include <sys/stat.h>
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
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"
#include "../../io/json/json_olc.h"
#include "../../io/json/json_rsg.h"

RSGEDIT(rsgedit_name);
RSGEDIT(rsgedit_description);
RSGEDIT(rsgedit_save);
RSGEDIT(rsgedit_reload);
RSGEDIT(rsgedit_pattern);
RSGEDIT(rsgedit_class);
RSGEDIT(rsgedit_generate);
RSGEDIT(rsgedit_list);
RSGEDIT(rsgedit_create);
RSGEDIT(rsgedit_show);
RSGEDIT(rsgedit_pattern_list);
RSGEDIT(rsgedit_pattern_create);
RSGEDIT(rsgedit_pattern_edit);
RSGEDIT(rsgedit_pattern_show);
RSGEDIT(rsgedit_pattern_delete);
RSGEDIT(rsgedit_pattern_help);
RSGEDIT(rsgedit_class_list);
RSGEDIT(rsgedit_class_create);
RSGEDIT(rsgedit_class_show);
RSGEDIT(rsgedit_class_delete);
RSGEDIT(rsgedit_class_add);
RSGEDIT(rsgedit_class_edit);
RSGEDIT(rsgedit_class_remove);
RSGEDIT(rsgedit_class_help);

static RANDOM_STRING *rsg_list = NULL;
static long rsg_next_uid = 1;
static bool rsg_cache_loaded = false;
static bool rsg_cache_dirty = false;

static void rsg_list_add(RANDOM_STRING *rsg);
static void rsg_free_generator(RANDOM_STRING *rsg);
static RANDOM_STRING *rsg_find_by_uid(long uid);
static RANDOM_STRING *rsg_find_by_name(const char *name);
static RANDOM_STRING *rsg_find_by_token(const char *token);
static RANDOM_PATTERN *rsg_pattern_find_by_name(RANDOM_STRING *rsg, const char *name);
static void rsgedit_show_patterns_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void rsgedit_show_classes_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void rsgedit_show_templates_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static bool rsg_parse_placeholder(const char *ptr, int *consumed, char *class_name, size_t class_name_size);
static void rsg_escape_template_for_display(const char *src, char *dst, size_t dst_size);
static bool rsg_use_mxp(CHAR_DATA *ch);
static void rsg_mxp_send_text(CHAR_DATA *ch, const char *command,
                              const char *text, char *out, size_t out_size);
static void rsg_trim_inplace(char *str);

static void rsg_trim_inplace(char *str)
{
    char *start;
    size_t len;

    if (!str)
        return;

    start = str;
    while (*start && isspace((unsigned char)*start))
        start++;

    if (start != str)
        memmove(str, start, strlen(start) + 1);

    len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1]))
        str[--len] = '\0';
}

static void rsg_history_key(const RANDOM_STRING *rsg, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0)
        return;

    if (!rsg || rsg->uid <= 0) {
        snprintf(buf, buf_size, "0");
        return;
    }

    snprintf(buf, buf_size, "%ld", rsg->uid);
}

static OLC_CHANGE_HISTORY *rsgedit_get_history(void *pEdit)
{
    RANDOM_STRING *rsg = (RANDOM_STRING *)pEdit;
    return rsg ? (OLC_CHANGE_HISTORY *)rsg->olc_history : NULL;
}

static OLC_CHANGE_HISTORY *rsgedit_ensure_history(RANDOM_STRING *rsg)
{
    char key[32];

    if (!rsg)
        return NULL;

    if (!rsg->olc_history) {
        rsg_history_key(rsg, key, sizeof(key));
        rsg->olc_history = olc_history_load(OLC_HIST_RSG, key);
        if (!rsg->olc_history)
            rsg->olc_history = olc_history_new();
    }

    return (OLC_CHANGE_HISTORY *)rsg->olc_history;
}

static void rsgedit_record(RANDOM_STRING *rsg, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    char key[32];

    if (!rsg)
        return;

    olc_history_record(rsgedit_ensure_history(rsg), ch, field, old_val, new_val);
    rsg_history_key(rsg, key, sizeof(key));
    olc_history_mark_dirty(OLC_HIST_RSG, key,
        (OLC_CHANGE_HISTORY *)rsg->olc_history);
}

static void rsgedit_record_cb(void *ctx, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    rsgedit_record((RANDOM_STRING *)ctx, ch, field, old_val, new_val);
}

static void rsg_free_entry(RANDOM_STRING_ENTRY *entry)
{
    if (!entry)
        return;

    free_string(entry->str);
    free_mem(entry, sizeof(*entry));
}

static void rsg_free_class(RANDOM_CLASS *cls)
{
    RANDOM_STRING_ENTRY *entry;
    RANDOM_STRING_ENTRY *entry_next;

    if (!cls)
        return;

    for (entry = cls->e_head; entry; entry = entry_next) {
        entry_next = entry->next;
        rsg_free_entry(entry);
    }

    free_string(cls->name);
    free_mem(cls, sizeof(*cls));
}

static void rsg_free_pattern(RANDOM_PATTERN *pattern)
{
    if (!pattern)
        return;

    free_string(pattern->name);
    free_string(pattern->template);
    if (pattern->c_list)
        free_mem(pattern->c_list, sizeof(RANDOM_CLASS *) * pattern->classes);
    free_mem(pattern, sizeof(*pattern));
}

static void rsg_free_generator(RANDOM_STRING *rsg)
{
    RANDOM_PATTERN *pattern;
    RANDOM_PATTERN *pattern_next;
    RANDOM_CLASS *cls;
    RANDOM_CLASS *cls_next;

    if (!rsg)
        return;

    for (pattern = rsg->p_head; pattern; pattern = pattern_next) {
        pattern_next = pattern->next;
        rsg_free_pattern(pattern);
    }

    for (cls = rsg->c_head; cls; cls = cls_next) {
        cls_next = cls->next;
        rsg_free_class(cls);
    }

    free_string(rsg->name);
    free_string(rsg->descr);
    if (rsg->olc_history)
        olc_history_free((OLC_CHANGE_HISTORY *)rsg->olc_history);
    free_mem(rsg, sizeof(*rsg));
}

static void rsg_clear_cache(void)
{
    RANDOM_STRING *rsg;
    RANDOM_STRING *rsg_next;

    for (rsg = rsg_list; rsg; rsg = rsg_next) {
        rsg_next = rsg->next;
        rsg_free_generator(rsg);
    }

    rsg_list = NULL;
    rsg_next_uid = 1;
    rsg_cache_dirty = false;
}

static RANDOM_STRING *rsg_find_by_uid(long uid)
{
    RANDOM_STRING *rsg;

    for (rsg = rsg_list; rsg; rsg = rsg->next)
        if (rsg->uid == uid)
            return rsg;

    return NULL;
}

static RANDOM_STRING *rsg_find_by_name(const char *name)
{
    RANDOM_STRING *rsg;

    if (IS_NULLSTR(name))
        return NULL;

    for (rsg = rsg_list; rsg; rsg = rsg->next) {
        if (!str_cmp(rsg->name, name))
            return rsg;
    }

    return NULL;
}

static RANDOM_STRING *rsg_find_by_token(const char *token)
{
    if (IS_NULLSTR(token))
        return NULL;

    if (is_number(token))
        return rsg_find_by_uid(atol(token));

    return rsg_find_by_name(token);
}

static RANDOM_CLASS *rsg_class_find_by_uid(RANDOM_STRING *rsg, long uid)
{
    RANDOM_CLASS *cls;

    if (!rsg)
        return NULL;

    for (cls = rsg->c_head; cls; cls = cls->next)
        if (cls->uid == uid)
            return cls;

    return NULL;
}

static RANDOM_CLASS *rsg_class_find_by_name(RANDOM_STRING *rsg, const char *name)
{
    RANDOM_CLASS *cls;

    if (!rsg || IS_NULLSTR(name))
        return NULL;

    for (cls = rsg->c_head; cls; cls = cls->next)
        if (!str_cmp(cls->name, name))
            return cls;

    return NULL;
}

static RANDOM_CLASS *rsg_class_resolve(RANDOM_STRING *rsg, const char *token)
{
    if (is_number(token))
        return rsg_class_find_by_uid(rsg, atol(token));

    return rsg_class_find_by_name(rsg, token);
}

static RANDOM_PATTERN *rsg_pattern_by_index(RANDOM_STRING *rsg, int index)
{
    RANDOM_PATTERN *pattern;
    int position = 1;

    if (!rsg || index < 1)
        return NULL;

    for (pattern = rsg->p_head; pattern; pattern = pattern->next, position++)
        if (position == index)
            return pattern;

    return NULL;
}

static RANDOM_PATTERN *rsg_pattern_find_by_name(RANDOM_STRING *rsg, const char *name)
{
    RANDOM_PATTERN *pattern;

    if (!rsg || IS_NULLSTR(name))
        return NULL;

    for (pattern = rsg->p_head; pattern; pattern = pattern->next)
        if (!str_cmp(pattern->name, name))
            return pattern;

    return NULL;
}

static RANDOM_PATTERN *rsg_pattern_resolve(RANDOM_STRING *rsg, const char *token)
{
    if (IS_NULLSTR(token))
        return NULL;

    if (is_number(token))
        return rsg_pattern_by_index(rsg, atoi(token));

    return rsg_pattern_find_by_name(rsg, token);
}

static RANDOM_STRING_ENTRY *rsg_class_entry_by_index(RANDOM_CLASS *cls, int index)
{
    RANDOM_STRING_ENTRY *entry;
    int position = 1;

    if (!cls || index < 1)
        return NULL;

    for (entry = cls->e_head; entry; entry = entry->next, position++)
        if (position == index)
            return entry;

    return NULL;
}

static void rsg_append_pattern(RANDOM_STRING *rsg, RANDOM_PATTERN *pattern)
{
    if (!rsg->p_head)
        rsg->p_head = pattern;
    else
        rsg->p_tail->next = pattern;

    rsg->p_tail = pattern;
    rsg->patterns++;
}

static void rsg_append_class(RANDOM_STRING *rsg, RANDOM_CLASS *cls)
{
    if (!rsg->c_head)
        rsg->c_head = cls;
    else
        rsg->c_tail->next = cls;

    rsg->c_tail = cls;
    rsg->classes++;
}

static void rsg_append_entry(RANDOM_CLASS *cls, RANDOM_STRING_ENTRY *entry)
{
    if (!cls->e_head)
        cls->e_head = entry;
    else
        cls->e_tail->next = entry;

    cls->e_tail = entry;
    cls->entries++;
}

static void rsg_pattern_rebuild_refs(RANDOM_STRING *rsg, RANDOM_PATTERN *pattern)
{
    char class_name[MIL];
    int consumed;
    int class_count = 0;
    RANDOM_CLASS *found;
    RANDOM_CLASS **new_refs = NULL;
    const char *ptr;

    if (!rsg || !pattern)
        return;

    if (pattern->c_list) {
        free_mem(pattern->c_list, sizeof(RANDOM_CLASS *) * pattern->classes);
        pattern->c_list = NULL;
    }
    pattern->classes = 0;

    ptr = pattern->template;
    while (*ptr) {
        int i = 0;

        if (*ptr == '{' && *(ptr + 1) == '{') {
            ptr += 2;
            continue;
        }

        if (*ptr != '{') {
            ptr++;
            continue;
        }

        if (!rsg_parse_placeholder(ptr, &consumed, class_name, sizeof(class_name))) {
            ptr++;
            continue;
        }
        ptr += consumed;

        found = rsg_class_find_by_name(rsg, class_name);
        if (!found)
            continue;

        bool already_added = false;
        for (i = 0; i < class_count; i++) {
            if (new_refs[i] == found) {
                already_added = true;
                break;
            }
        }

        if (already_added)
            continue;

        RANDOM_CLASS **grown = alloc_mem(sizeof(RANDOM_CLASS *) * (class_count + 1));
        for (i = 0; i < class_count; i++)
            grown[i] = new_refs[i];
        grown[class_count++] = found;

        if (new_refs)
            free_mem(new_refs, sizeof(RANDOM_CLASS *) * (class_count - 1));
        new_refs = grown;
    }

    pattern->classes = class_count;
    pattern->c_list = new_refs;
}

static void rsg_rebuild_all_pattern_refs(RANDOM_STRING *rsg)
{
    RANDOM_PATTERN *pattern;

    if (!rsg)
        return;

    for (pattern = rsg->p_head; pattern; pattern = pattern->next)
        rsg_pattern_rebuild_refs(rsg, pattern);
}

static bool rsg_mark_dirty_and_save(void)
{
    rsg_cache_dirty = true;
    return save_rsg_data();
}

bool save_rsg_data(void)
{
    int saved = 0;

    if (!rsg_cache_loaded)
        return true;

    if (!json_rsg_save_generators(rsg_list, rsg_next_uid, &saved))
        return false;

    rsg_cache_dirty = false;
    log_stringf("save_rsg_data: Saved %d random string generators.", saved);
    return true;
}

void load_rsg_data(void)
{
    RANDOM_STRING *loaded = NULL;
    int loaded_count = 0;

    if (rsg_cache_loaded)
        return;

    rsg_cache_loaded = true;
    rsg_clear_cache();

    loaded = json_rsg_load_generators(&rsg_next_uid, &loaded_count);
    rsg_list = loaded;

    rsg_cache_dirty = false;
    log_stringf("load_rsg_data: Loaded %d random string generators.", loaded_count);
}

static void rsg_ensure_cache_loaded(void)
{
    if (!rsg_cache_loaded)
        load_rsg_data();
}

static void rsg_list_add(RANDOM_STRING *rsg)
{
    if (!rsg)
        return;

    rsg->next = rsg_list;
    rsg_list = rsg;
}

static RANDOM_STRING *rsg_create_new(const char *name)
{
    RANDOM_STRING *rsg;

    if (IS_NULLSTR(name))
        return NULL;

    if (rsg_find_by_name(name))
        return NULL;

    rsg = alloc_mem(sizeof(*rsg));
    memset(rsg, 0, sizeof(*rsg));

    rsg->uid = rsg_next_uid++;
    rsg->name = str_dup(name);
    rsg->descr = str_dup("");

    rsg_list_add(rsg);
    return rsg;
}

const struct olc_cmd_type rsgedit_table[] = {
    { "?",          show_help            },
    { "class",      rsgedit_class        },
    { "commands",   show_commands        },
    { "create",     rsgedit_create       },
    { "description",rsgedit_description  },
    { "generate",   rsgedit_generate     },
    { "list",       rsgedit_list         },
    { "name",       rsgedit_name         },
    { "pattern",    rsgedit_pattern      },
    { "reload",     rsgedit_reload       },
    { "save",       rsgedit_save         },
    { "show",       rsgedit_show         },
    { NULL,          0                    }
};

const struct olc_cmd_type rsgedit_pattern_table[] = {
    { "?",          rsgedit_pattern_help   },
    { "create",     rsgedit_pattern_create },
    { "edit",       rsgedit_pattern_edit   },
    { "delete",     rsgedit_pattern_delete },
    { "list",       rsgedit_pattern_list   },
    { "show",       rsgedit_pattern_show   },
    { NULL,          0                      }
};

const struct olc_cmd_type rsgedit_class_table[] = {
    { "?",          rsgedit_class_help   },
    { "add",        rsgedit_class_add    },
    { "create",     rsgedit_class_create },
    { "delete",     rsgedit_class_delete },
    { "edit",       rsgedit_class_edit   },
    { "list",       rsgedit_class_list   },
    { "remove",     rsgedit_class_remove },
    { "show",       rsgedit_class_show   },
    { NULL,          0                    }
};

static const OLC_EDITOR_DEF rsgedit_def = {
    .name           = "RSGEdit",
    .editor_type    = ED_RSG,
    .cmd_table      = rsgedit_table,
    .show_fn        = rsgedit_show,
    .tabs           = {
        .count      = 3,
        .tabs       = {
            { "Patterns",  "Pat", rsgedit_show_patterns_tab  },
            { "Classes",   "Cls", rsgedit_show_classes_tab   },
            { "Templates", "Tpl", rsgedit_show_templates_tab },
        },
    },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = true,
    .get_history_fn = rsgedit_get_history,
};

void do_rsgedit(CHAR_DATA *ch, char *argument)
{
    RANDOM_STRING *rsg;
    char arg1[MAX_INPUT_LENGTH];

    if (IS_NPC(ch))
        return;

    rsg_ensure_cache_loaded();

    if (!olc_editor_check_perm(ch, &rsgedit_def, NULL)) {
        send_to_char("You don't have permission to edit random string generators.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: rsgedit list\n\r", ch);
        send_to_char("        rsgedit create <name>\n\r", ch);
        send_to_char("        rsgedit <uid|name>\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "list")) {
        rsgedit_list(ch, argument);
        return;
    }

    if (!str_cmp(arg1, "reload")) {
        rsg_cache_loaded = false;
        rsg_clear_cache();
        load_rsg_data();
        send_to_char("RSG cache reloaded from disk.\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "create")) {
        if (IS_NULLSTR(argument)) {
            send_to_char("Syntax: rsgedit create <name>\n\r", ch);
            return;
        }

        rsg = rsg_create_new(argument);
        if (!rsg) {
            send_to_char("That generator already exists (or the name is invalid).\n\r", ch);
            return;
        }

        rsgedit_record(rsg, ch, "generator", "", "created");
        send_to_char("Random string generator created.\n\r", ch);
        olc_editor_enter(ch, &rsgedit_def, rsg, true);
        return;
    }

    if (is_number(arg1))
        rsg = rsg_find_by_uid(atol(arg1));
    else
        rsg = rsg_find_by_name(arg1);

    if (!rsg) {
        send_to_char("No random string generator by that uid or name.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &rsgedit_def, rsg, true);
}

void rsgedit(CHAR_DATA *ch, char *argument)
{
    rsg_ensure_cache_loaded();
    olc_editor_interp(ch, argument, &rsgedit_def);
}

RSGEDIT(rsgedit_list)
{
    BUFFER *buffer;
    RANDOM_STRING *rsg;
    bool append_ok = true;

    if (!rsg_list) {
        send_to_char("No random string generators exist yet.\n\r", ch);
        return false;
    }

    buffer = new_buf();
    if (!add_buf(buffer, "{WUID   Name{X\n\r")
        || !add_buf(buffer, "{D----- ------------------------------{X\n\r")) {
        send_to_char("Random string generator list exceeded buffer limits.\n\r", ch);
        free_buf(buffer);
        return false;
    }

    for (rsg = rsg_list; rsg; rsg = rsg->next) {
        if (!add_buf(buffer, formatf("{W%-5ld {x%s{X\n\r", rsg->uid, rsg->name))) {
            append_ok = false;
            break;
        }
    }

    if (!append_ok) {
        send_to_char("Random string generator list exceeded buffer limits.\n\r", ch);
        free_buf(buffer);
        return false;
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return false;
}

RSGEDIT(rsgedit_name)
{
    RANDOM_STRING *rsg;
    bool changed;

    EDIT_RSG(ch, rsg);

    if (!IS_NULLSTR(argument)) {
        RANDOM_STRING *existing = rsg_find_by_name(argument);
        if (existing && existing != rsg) {
            send_to_char("Another random string generator already has that name.\n\r", ch);
            return false;
        }
    }

    changed = olc_cmd_string(ch, argument, "Name", "Syntax: name <new name>",
        &rsg->name, OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, rsg, rsgedit_record_cb);
    if (!changed)
        return false;

    if (!rsg_mark_dirty_and_save()) {
        send_to_char("Name updated, but failed to save rsg.json.\n\r", ch);
        return true;
    }

    return true;
}

RSGEDIT(rsgedit_description)
{
    RANDOM_STRING *rsg;
    bool changed;

    EDIT_RSG(ch, rsg);

    if (IS_NULLSTR(argument)) {
        changed = olc_cmd_string_append(ch, argument, "Description", NULL,
            &rsg->descr, rsg, rsgedit_record_cb);
    } else {
        changed = olc_cmd_string(ch, argument, "Description", NULL,
            &rsg->descr, OLC_STR_DEFAULT, rsg, rsgedit_record_cb);
    }

    if (!changed)
        return false;

    if (!rsg_mark_dirty_and_save()) {
        send_to_char("Description updated, but failed to save rsg.json.\n\r", ch);
        return true;
    }

    return true;
}

RSGEDIT(rsgedit_save)
{
    RANDOM_STRING *rsg;
    char key[32];

    EDIT_RSG(ch, rsg);

    if (save_rsg_data()) {
        if (rsg && rsg->olc_history) {
            rsg_history_key(rsg, key, sizeof(key));
            olc_history_flush(OLC_HIST_RSG, key,
                (OLC_CHANGE_HISTORY *)rsg->olc_history);
        }
        send_to_char("Random string generator data saved.\n\r", ch);
        return false;
    }

    send_to_char("Failed to save random string generator data.\n\r", ch);
    return false;
}

RSGEDIT(rsgedit_reload)
{
    RANDOM_STRING *rsg;
    RANDOM_STRING *reloaded = NULL;
    long uid = 0;

    EDIT_RSG(ch, rsg);

    if (rsg)
        uid = rsg->uid;

    rsg_cache_loaded = false;
    rsg_clear_cache();
    load_rsg_data();

    if (uid > 0)
        reloaded = rsg_find_by_uid(uid);

    if (!reloaded) {
        send_to_char("RSG cache reloaded, but current generator no longer exists.\n\r", ch);
        edit_done(ch);
        return false;
    }

    if (ch->desc)
        ch->desc->pEdit = reloaded;

    send_to_char("RSG cache reloaded from disk.\n\r", ch);
    return false;
}

RSGEDIT(rsgedit_create)
{
    RANDOM_STRING *rsg;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: create <name>\n\r", ch);
        return false;
    }

    rsg = rsg_create_new(argument);
    if (!rsg) {
        send_to_char("That generator already exists (or the name is invalid).\n\r", ch);
        return false;
    }

    rsgedit_record(rsg, ch, "generator", "", "created");
    send_to_char("Random string generator created.\n\r", ch);
    olc_editor_enter(ch, &rsgedit_def, rsg, true);
    return true;
}

RSGEDIT(rsgedit_show)
{
    RANDOM_STRING *rsg;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&rsgedit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    EDIT_RSG(ch, rsg);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "RSGEdit", rsg->name,
        formatf("UID %ld", rsg->uid), &rsgedit_def);

    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        for (int i = 0; i < rsgedit_def.tabs.count; i++) {
            if (rsgedit_def.tabs.tabs[i].show_fn)
                rsgedit_def.tabs.tabs[i].show_fn(ch, ctx, (void *)rsg);
        }
    } else if (tab >= 0 && tab < rsgedit_def.tabs.count
        && rsgedit_def.tabs.tabs[tab].show_fn) {
        rsgedit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)rsg);
    } else {
        rsgedit_show_patterns_tab(ch, ctx, (void *)rsg);
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

static void rsgedit_show_patterns_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    RANDOM_STRING *rsg = (RANDOM_STRING *)pEdit;
    RANDOM_PATTERN *pattern;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&rsgedit_def);
    int index = 1;
    char cmd_list[MIL], cmd_create[MIL], cmd_edit[MIL], cmd_show[MIL], cmd_delete[MIL];

    rsg_mxp_send_text(ch, "pattern list", "pattern list", cmd_list, sizeof(cmd_list));
    rsg_mxp_send_text(ch, "pattern create", "pattern create", cmd_create, sizeof(cmd_create));
    rsg_mxp_send_text(ch, "pattern edit", "pattern edit", cmd_edit, sizeof(cmd_edit));
    rsg_mxp_send_text(ch, "pattern show", "pattern show", cmd_show, sizeof(cmd_show));
    rsg_mxp_send_text(ch, "pattern delete", "pattern delete", cmd_delete, sizeof(cmd_delete));

    olc_display_string(ctx, theme, "Name:", "name", rsg->name);
    olc_display_text(ctx, theme, "Description:", "description",
        IS_NULLSTR(rsg->descr) ? NULL : rsg->descr);
    olc_display_pair(ctx, theme,
        "Patterns:", NULL, formatf("%ld", rsg->patterns),
        "Classes:", NULL, formatf("%ld", rsg->classes));

    olc_display_section(ctx, theme, "Pattern List");
    if (!rsg->p_head) {
        add_buf(ctx->buffer, "(no patterns defined)\n\r");
    } else {
        add_buf(ctx->buffer, "{WIdx  Weight  ClassRefs  Name               Template{X\n\r");
        add_buf(ctx->buffer, "{D---  ------  ---------  ------------------ ------------------------------{X\n\r");
        for (pattern = rsg->p_head; pattern; pattern = pattern->next, index++) {
            char cmd[MIL];
            char idx_txt[32];
            char idx_disp[MIL];
            char name_escaped[MSL];
            char templ_escaped[MSL];
            char templ_disp[MSL];

            snprintf(cmd, sizeof(cmd), "pattern show %d", index);
            snprintf(idx_txt, sizeof(idx_txt), "%3d", index);
            rsg_mxp_send_text(ch, cmd, idx_txt, idx_disp, sizeof(idx_disp));
            rsg_escape_template_for_display(pattern->name ? pattern->name : "",
                name_escaped, sizeof(name_escaped));
            rsg_escape_template_for_display(pattern->template ? pattern->template : "",
                templ_escaped, sizeof(templ_escaped));
            rsg_mxp_send_text(ch, cmd, templ_escaped, templ_disp, sizeof(templ_disp));

            add_buf(ctx->buffer, formatf("{W%s  %6d  %9d  {x%-18.18s {x%s{X\n\r",
                idx_disp,
                pattern->weight,
                pattern->classes,
                name_escaped,
                templ_disp));
        }
    }

    olc_display_infof(ctx, theme,
            "%sCommands:%s %s | %s | %s | %s | %s",
            theme->label, theme->value, cmd_list, cmd_create, cmd_edit, cmd_show, cmd_delete);
}

static void rsgedit_show_classes_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    RANDOM_STRING *rsg = (RANDOM_STRING *)pEdit;
    RANDOM_CLASS *cls;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&rsgedit_def);
    char cmd_list[MIL], cmd_create[MIL], cmd_show[MIL], cmd_delete[MIL], cmd_add[MIL], cmd_edit[MIL], cmd_remove[MIL];

    rsg_mxp_send_text(ch, "class list", "class list", cmd_list, sizeof(cmd_list));
    rsg_mxp_send_text(ch, "class create", "class create", cmd_create, sizeof(cmd_create));
    rsg_mxp_send_text(ch, "class show", "class show", cmd_show, sizeof(cmd_show));
    rsg_mxp_send_text(ch, "class delete", "class delete", cmd_delete, sizeof(cmd_delete));
    rsg_mxp_send_text(ch, "class add", "class add", cmd_add, sizeof(cmd_add));
    rsg_mxp_send_text(ch, "class edit", "class edit", cmd_edit, sizeof(cmd_edit));
    rsg_mxp_send_text(ch, "class remove", "class remove", cmd_remove, sizeof(cmd_remove));

    olc_display_section(ctx, theme, "Class List");
    if (!rsg->c_head) {
        add_buf(ctx->buffer, "(no classes defined)\n\r");
    } else {
        add_buf(ctx->buffer, "{WUID    Entries  Name{X\n\r");
        add_buf(ctx->buffer, "{D-----  -------  ------------------------------{X\n\r");
        for (cls = rsg->c_head; cls; cls = cls->next) {
            char cmd[MIL];
            char uid_txt[32];
            char uid_disp[MIL];

            snprintf(cmd, sizeof(cmd), "class show %ld", cls->uid);
            snprintf(uid_txt, sizeof(uid_txt), "%-5ld", cls->uid);
            rsg_mxp_send_text(ch, cmd, uid_txt, uid_disp, sizeof(uid_disp));

            add_buf(ctx->buffer, formatf("{W%s  %-7ld  {x%s{X\n\r",
                uid_disp,
                cls->entries,
                cls->name ? cls->name : ""));
        }
    }

    olc_display_infof(ctx, theme,
        "%sCommands:%s %s | %s | %s | %s | %s | %s | %s",
        theme->label, theme->value,
        cmd_list, cmd_create, cmd_show, cmd_delete, cmd_add, cmd_edit, cmd_remove);
}

static void rsgedit_show_templates_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    RANDOM_STRING *rsg = (RANDOM_STRING *)pEdit;
    RANDOM_PATTERN *pattern;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&rsgedit_def);
    int index = 1;
    char cmd_gen[MIL];

    rsg_mxp_send_text(ch, "generate", "generate [count]", cmd_gen, sizeof(cmd_gen));

    olc_display_section(ctx, theme, "Template Preview");
    if (!rsg->p_head) {
        add_buf(ctx->buffer, "(no templates available)\n\r");
    } else {
        for (pattern = rsg->p_head; pattern; pattern = pattern->next, index++) {
            char cmd[MIL];
            char label[32];
            char idx_disp[MIL];
            char templ_escaped[MSL];
            char templ_disp[MSL];

            snprintf(cmd, sizeof(cmd), "pattern show %d", index);
            snprintf(label, sizeof(label), "[%d]", index);
            rsg_mxp_send_text(ch, cmd, label, idx_disp, sizeof(idx_disp));
            rsg_escape_template_for_display(pattern->template ? pattern->template : "",
                templ_escaped, sizeof(templ_escaped));
            rsg_mxp_send_text(ch, cmd, templ_escaped, templ_disp, sizeof(templ_disp));

            add_buf(ctx->buffer, formatf("{W%s{x %s\n\r", idx_disp, templ_disp));
        }
    }

    olc_display_infof(ctx, theme,
        "%sGenerate:%s %s",
        theme->label, theme->value, cmd_gen);
}

static bool rsg_parse_placeholder(const char *ptr, int *consumed, char *class_name, size_t class_name_size)
{
    const char *p;
    size_t len = 0;

    if (!ptr || ptr[0] != '{')
        return false;

    if (ptr[1] == '{')
        return false;

    p = ptr + 1;
    if (!*p || !(isalpha((unsigned char)*p) || *p == '_'))
        return false;

    while (*p && *p != '}') {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-'))
            return false;

        if (class_name && class_name_size > 0 && len < class_name_size - 1)
            class_name[len] = *p;
        len++;
        p++;
    }

    if (*p != '}' || len == 0)
        return false;

    if (class_name && class_name_size > 0) {
        if (len >= class_name_size)
            len = class_name_size - 1;
        class_name[len] = '\0';
    }

    if (consumed)
        *consumed = (int)(p - ptr + 1);

    return true;
}

static void rsg_escape_template_for_display(const char *src, char *dst, size_t dst_size)
{
    size_t di = 0;

    if (!dst || dst_size == 0)
        return;

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (*src && di < dst_size - 1) {
        if (*src == '{' && di + 2 < dst_size) {
            dst[di++] = '{';
            dst[di++] = '{';
            src++;
            continue;
        }

        dst[di++] = *src++;
    }

    dst[di] = '\0';
}

static bool rsg_use_mxp(CHAR_DATA *ch)
{
    if (!ch || !ch->desc)
        return false;

    return isMXP(ch->desc) && IS_SET(ch->comm, COMM_MXP);
}

static void rsg_mxp_send_text(CHAR_DATA *ch, const char *command,
                              const char *text, char *out, size_t out_size)
{
    const char *mxp;

    if (!out || out_size == 0)
        return;

    if (!text)
        text = "";

    if (!command || command[0] == '\0' || !rsg_use_mxp(ch)) {
        snprintf(out, out_size, "%s", text);
        return;
    }

    mxp = MXPCreateSend(ch->desc, command, text);
    snprintf(out, out_size, "%s", mxp ? mxp : text);
}

static RANDOM_STRING_ENTRY *rsg_pick_weighted_entry(RANDOM_CLASS *cls)
{
    RANDOM_STRING_ENTRY *entry;
    int total_weight = 0;
    int roll;

    if (!cls || !cls->e_head)
        return NULL;

    for (entry = cls->e_head; entry; entry = entry->next)
        total_weight += UMAX(1, entry->weight);

    if (total_weight <= 0)
        return cls->e_head;

    roll = number_range(1, total_weight);
    for (entry = cls->e_head; entry; entry = entry->next) {
        roll -= UMAX(1, entry->weight);
        if (roll <= 0)
            return entry;
    }

    return cls->e_tail ? cls->e_tail : cls->e_head;
}

static RANDOM_PATTERN *rsg_pick_weighted_pattern(RANDOM_STRING *rsg)
{
    RANDOM_PATTERN *pattern;
    int total_weight = 0;
    int roll;

    if (!rsg || !rsg->p_head)
        return NULL;

    for (pattern = rsg->p_head; pattern; pattern = pattern->next)
        total_weight += UMAX(1, pattern->weight);

    if (total_weight <= 0)
        return rsg->p_head;

    roll = number_range(1, total_weight);
    for (pattern = rsg->p_head; pattern; pattern = pattern->next) {
        roll -= UMAX(1, pattern->weight);
        if (roll <= 0)
            return pattern;
    }

    return rsg->p_tail ? rsg->p_tail : rsg->p_head;
}

static void rsg_safe_append(char *out, size_t out_size, const char *src)
{
    size_t used;
    size_t free_space;

    if (!out || !out_size || !src)
        return;

    used = strlen(out);
    if (used >= out_size - 1)
        return;

    free_space = out_size - used - 1;
    strncat(out, src, free_space);
}

static void rsg_generate_from_template(RANDOM_STRING *rsg, const char *templ,
                                       char *out, size_t out_size)
{
    const char *ptr = templ;

    out[0] = '\0';

    while (*ptr && strlen(out) < out_size - 1) {
        char class_name[MIL];
        int consumed = 0;

        if (*ptr == '{' && *(ptr + 1) == '{') {
            rsg_safe_append(out, out_size, "{");
            ptr += 2;
            continue;
        }

        if (!rsg_parse_placeholder(ptr, &consumed, class_name, sizeof(class_name))) {
            char chunk[2] = { *ptr, '\0' };
            rsg_safe_append(out, out_size, chunk);
            ptr++;
            continue;
        }
        ptr += consumed;

        RANDOM_CLASS *cls = rsg_class_find_by_name(rsg, class_name);
        RANDOM_STRING_ENTRY *entry = rsg_pick_weighted_entry(cls);

        if (entry && !IS_NULLSTR(entry->str))
            rsg_safe_append(out, out_size, entry->str);
        else {
            rsg_safe_append(out, out_size, "{");
            rsg_safe_append(out, out_size, class_name);
            rsg_safe_append(out, out_size, "}");
        }
    }
}

bool rsg_generate_any(const char *generator_token, char *out, size_t out_size)
{
    RANDOM_STRING *rsg;
    RANDOM_PATTERN *pattern;

    if (!out || out_size == 0)
        return false;

    out[0] = '\0';
    if (IS_NULLSTR(generator_token))
        return false;

    rsg_ensure_cache_loaded();
    rsg = rsg_find_by_token(generator_token);
    if (!rsg || !rsg->p_head)
        return false;

    pattern = rsg_pick_weighted_pattern(rsg);
    if (!pattern || IS_NULLSTR(pattern->template))
        return false;

    rsg_generate_from_template(rsg, pattern->template, out, out_size);
    return !IS_NULLSTR(out);
}

bool rsg_generate_pattern(const char *generator_token, int pattern_index,
    char *out, size_t out_size)
{
    RANDOM_STRING *rsg;
    RANDOM_PATTERN *pattern;

    if (!out || out_size == 0)
        return false;

    out[0] = '\0';
    if (IS_NULLSTR(generator_token) || pattern_index < 1)
        return false;

    rsg_ensure_cache_loaded();
    rsg = rsg_find_by_token(generator_token);
    if (!rsg)
        return false;

    pattern = rsg_pattern_by_index(rsg, pattern_index);
    if (!pattern || IS_NULLSTR(pattern->template))
        return false;

    rsg_generate_from_template(rsg, pattern->template, out, out_size);
    return !IS_NULLSTR(out);
}

bool rsg_generate_pattern_named(const char *generator_token, const char *pattern_name,
    char *out, size_t out_size)
{
    RANDOM_STRING *rsg;
    RANDOM_PATTERN *pattern;

    if (!out || out_size == 0)
        return false;

    out[0] = '\0';
    if (IS_NULLSTR(generator_token) || IS_NULLSTR(pattern_name))
        return false;

    rsg_ensure_cache_loaded();
    rsg = rsg_find_by_token(generator_token);
    if (!rsg)
        return false;

    pattern = rsg_pattern_find_by_name(rsg, pattern_name);
    if (!pattern || IS_NULLSTR(pattern->template))
        return false;

    rsg_generate_from_template(rsg, pattern->template, out, out_size);
    return !IS_NULLSTR(out);
}

bool rsg_generate_template_named(const char *generator_token, const char *template_name,
    char *out, size_t out_size)
{
    return rsg_generate_pattern_named(generator_token, template_name, out, out_size);
}

bool rsg_generate_class(const char *generator_token, const char *class_name,
    char *out, size_t out_size)
{
    RANDOM_STRING *rsg;
    RANDOM_CLASS *cls;
    RANDOM_STRING_ENTRY *entry;

    if (!out || out_size == 0)
        return false;

    out[0] = '\0';
    if (IS_NULLSTR(generator_token) || IS_NULLSTR(class_name))
        return false;

    rsg_ensure_cache_loaded();
    rsg = rsg_find_by_token(generator_token);
    if (!rsg)
        return false;

    cls = rsg_class_find_by_name(rsg, class_name);
    if (!cls)
        return false;

    entry = rsg_pick_weighted_entry(cls);
    if (!entry || IS_NULLSTR(entry->str))
        return false;

    snprintf(out, out_size, "%s", entry->str);
    return true;
}

bool rsg_generate_template(const char *generator_token, const char *templ,
    char *out, size_t out_size)
{
    RANDOM_STRING *rsg;

    if (!out || out_size == 0)
        return false;

    out[0] = '\0';
    if (IS_NULLSTR(generator_token) || IS_NULLSTR(templ))
        return false;

    rsg_ensure_cache_loaded();
    rsg = rsg_find_by_token(generator_token);
    if (!rsg)
        return false;

    rsg_generate_from_template(rsg, templ, out, out_size);
    return !IS_NULLSTR(out);
}

bool rsg_generate_spec(const char *spec, char *out, size_t out_size)
{
    char work[MSL];
    char *generator;
    char *mode;
    char *param;
    char *first_colon;
    char *second_colon;

    if (!out || out_size == 0)
        return false;

    out[0] = '\0';
    if (IS_NULLSTR(spec))
        return false;

    snprintf(work, sizeof(work), "%s", spec);
    rsg_trim_inplace(work);
    if (IS_NULLSTR(work))
        return false;

    generator = work;
    while (*generator && isspace((unsigned char)*generator))
        generator++;
    if (*generator == '^')
        generator++;

    first_colon = strchr(generator, ':');
    if (!first_colon)
        return rsg_generate_any(generator, out, out_size);

    *first_colon = '\0';
    mode = first_colon + 1;
    second_colon = strchr(mode, ':');
    if (second_colon) {
        *second_colon = '\0';
        param = second_colon + 1;
    } else {
        param = NULL;
    }

    rsg_trim_inplace(generator);
    rsg_trim_inplace(mode);
    if (param)
        rsg_trim_inplace(param);

    if (IS_NULLSTR(generator) || IS_NULLSTR(mode))
        return false;

    if (!str_cmp(mode, "any"))
        return rsg_generate_any(generator, out, out_size);

    if (!str_cmp(mode, "pattern")) {
        if (param && is_number(param))
            return rsg_generate_pattern(generator, atoi(param), out, out_size);
        if (!IS_NULLSTR(param))
            return rsg_generate_pattern_named(generator, param, out, out_size);
        return rsg_generate_any(generator, out, out_size);
    }

    if (!str_cmp(mode, "template")) {
        if (IS_NULLSTR(param))
            return false;

        if (rsg_generate_template_named(generator, param, out, out_size))
            return true;

        // Backward compatibility: allow literal inline template text.
        return rsg_generate_template(generator, param, out, out_size);
    }

    if (!str_cmp(mode, "class")) {
        if (IS_NULLSTR(param))
            return false;
        return rsg_generate_class(generator, param, out, out_size);
    }

    return false;
}

RSGEDIT(rsgedit_generate)
{
    RANDOM_STRING *rsg;
    int count = 1;
    int i;

    EDIT_RSG(ch, rsg);

    if (!IS_NULLSTR(argument) && is_number(argument))
        count = atoi(argument);

    if (count < 1)
        count = 1;
    if (count > 20)
        count = 20;

    if (!rsg->p_head) {
        send_to_char("No patterns defined. Use 'pattern create <weight> <name> <template>'.\n\r", ch);
        return false;
    }

    for (i = 0; i < count; i++) {
        RANDOM_PATTERN *pattern = rsg_pick_weighted_pattern(rsg);
        char out[MSL];

        if (!pattern || IS_NULLSTR(pattern->template))
            continue;

        rsg_generate_from_template(rsg, pattern->template, out, sizeof(out));
        send_to_char(formatf("Generated [%2d]: {W%s{X\n\r", i + 1, out), ch);
    }

    return false;
}

RSGEDIT(rsgedit_pattern)
{
    char cmd[MIL];
    int i;

    argument = one_argument(argument, cmd);

    if (IS_NULLSTR(cmd)) {
        rsgedit_pattern_help(ch, "");
        return false;
    }

    for (i = 0; rsgedit_pattern_table[i].name != NULL; i++) {
        if (!str_prefix(cmd, rsgedit_pattern_table[i].name)) {
            (*rsgedit_pattern_table[i].olc_fun)(ch, argument);
            return false;
        }
    }

    send_to_char("Unknown pattern subcommand. Type 'pattern ?'.\n\r", ch);
    return false;
}

RSGEDIT(rsgedit_class)
{
    char cmd[MIL];
    int i;

    argument = one_argument(argument, cmd);

    if (IS_NULLSTR(cmd)) {
        rsgedit_class_help(ch, "");
        return false;
    }

    for (i = 0; rsgedit_class_table[i].name != NULL; i++) {
        if (!str_prefix(cmd, rsgedit_class_table[i].name)) {
            (*rsgedit_class_table[i].olc_fun)(ch, argument);
            return false;
        }
    }

    send_to_char("Unknown class subcommand. Type 'class ?'.\n\r", ch);
    return false;
}

RSGEDIT(rsgedit_pattern_list)
{
    RANDOM_STRING *rsg;
    RANDOM_PATTERN *pattern;
    int index = 1;

    EDIT_RSG(ch, rsg);

    if (!rsg->p_head) {
        send_to_char("No patterns defined.\n\r", ch);
        return false;
    }

    send_to_char("{WIdx  Weight  Name               Template{X\n\r", ch);
    send_to_char("{D---  ------  ------------------ ----------------------------------------------{X\n\r", ch);
    for (pattern = rsg->p_head; pattern; pattern = pattern->next, index++) {
        char name_escaped[MSL];
        char templ_escaped[MSL];
        rsg_escape_template_for_display(pattern->name, name_escaped, sizeof(name_escaped));
        rsg_escape_template_for_display(pattern->template, templ_escaped, sizeof(templ_escaped));
        send_to_char(formatf("{W%3d  %6d  {x%-18.18s {x%s{X\n\r",
            index,
            pattern->weight,
            name_escaped,
            templ_escaped), ch);
    }

    return false;
}

RSGEDIT(rsgedit_pattern_create)
{
    RANDOM_STRING *rsg;
    RANDOM_PATTERN *pattern;
    char weight_arg[MIL];
    char name_arg[MIL];
    int weight;

    EDIT_RSG(ch, rsg);

    argument = one_argument(argument, weight_arg);
    argument = one_argument(argument, name_arg);
    if (IS_NULLSTR(weight_arg) || !is_number(weight_arg) || IS_NULLSTR(name_arg) || IS_NULLSTR(argument)) {
        send_to_char("Syntax: pattern create <weight> <name> <template>\n\r", ch);
        send_to_char("Example: pattern create 100 classic_name {prefix}{root}{suffix}\n\r", ch);
        send_to_char("Use '{{' to emit a literal '{' inside templates.\n\r", ch);
        return false;
    }

    if (rsg_pattern_find_by_name(rsg, name_arg)) {
        send_to_char("A pattern with that name already exists.\n\r", ch);
        return false;
    }

    weight = atoi(weight_arg);
    if (weight < 1)
        weight = 1;

    pattern = alloc_mem(sizeof(*pattern));
    memset(pattern, 0, sizeof(*pattern));
    pattern->weight = weight;
    pattern->name = str_dup(name_arg);
    pattern->template = str_dup(argument);

    rsg_append_pattern(rsg, pattern);
    rsg_pattern_rebuild_refs(rsg, pattern);
    rsgedit_record(rsg, ch, "pattern", "",
        formatf("%s: %s", pattern->name, pattern->template));

    if (!rsg_mark_dirty_and_save())
        send_to_char("Pattern created, but failed to save rsg.json.\n\r", ch);
    else
        send_to_char("Pattern created.\n\r", ch);

    return true;
}

RSGEDIT(rsgedit_pattern_edit)
{
    RANDOM_STRING *rsg;
    RANDOM_PATTERN *pattern;
    char target_arg[MIL];
    char field_arg[MIL];
    char old_text[MSL];
    char new_text[MSL];
    bool rebuild_refs = false;

    EDIT_RSG(ch, rsg);

    argument = one_argument(argument, target_arg);
    argument = one_argument(argument, field_arg);

    if (IS_NULLSTR(target_arg) || IS_NULLSTR(field_arg) || IS_NULLSTR(argument)) {
        send_to_char("Syntax: pattern edit <index|name> <field> <value>\n\r", ch);
        send_to_char("Fields: weight | name | template\n\r", ch);
        return false;
    }

    pattern = rsg_pattern_resolve(rsg, target_arg);
    if (!pattern) {
        send_to_char("No pattern by that index or name.\n\r", ch);
        return false;
    }

    if (!str_prefix(field_arg, "weight")) {
        int weight;

        if (!is_number(argument)) {
            send_to_char("Weight must be a number.\n\r", ch);
            return false;
        }

        weight = atoi(argument);
        if (weight < 1)
            weight = 1;

        snprintf(old_text, sizeof(old_text), "%d", pattern->weight);
        pattern->weight = weight;
        snprintf(new_text, sizeof(new_text), "%d", pattern->weight);
        rsgedit_record(rsg, ch, "pattern weight", old_text, new_text);
    } else if (!str_prefix(field_arg, "name")) {
        RANDOM_PATTERN *name_conflict;

        name_conflict = rsg_pattern_find_by_name(rsg, argument);
        if (name_conflict && name_conflict != pattern) {
            send_to_char("Another pattern already has that name.\n\r", ch);
            return false;
        }

        snprintf(old_text, sizeof(old_text), "%s", pattern->name ? pattern->name : "");
        free_string(pattern->name);
        pattern->name = str_dup(argument);
        snprintf(new_text, sizeof(new_text), "%s", pattern->name ? pattern->name : "");
        rsgedit_record(rsg, ch, "pattern name", old_text, new_text);
    } else if (!str_prefix(field_arg, "template")) {
        snprintf(old_text, sizeof(old_text), "%s", pattern->template ? pattern->template : "");
        free_string(pattern->template);
        pattern->template = str_dup(argument);
        snprintf(new_text, sizeof(new_text), "%s", pattern->template ? pattern->template : "");
        rsgedit_record(rsg, ch, "pattern template", old_text, new_text);
        rebuild_refs = true;
    } else {
        send_to_char("Unknown field. Valid fields: weight, name, template\n\r", ch);
        return false;
    }

    if (rebuild_refs)
        rsg_pattern_rebuild_refs(rsg, pattern);

    if (!rsg_mark_dirty_and_save())
        send_to_char("Pattern updated, but failed to save rsg.json.\n\r", ch);
    else
        send_to_char("Pattern updated.\n\r", ch);

    return true;
}

RSGEDIT(rsgedit_pattern_show)
{
    RANDOM_STRING *rsg;
    RANDOM_PATTERN *pattern;
    int index;
    int i;

    EDIT_RSG(ch, rsg);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: pattern show <index|name>\n\r", ch);
        return false;
    }

    pattern = rsg_pattern_resolve(rsg, argument);
    if (!pattern) {
        send_to_char("No pattern by that index or name.\n\r", ch);
        return false;
    }

    index = 1;
    for (RANDOM_PATTERN *scan = rsg->p_head; scan; scan = scan->next, index++)
        if (scan == pattern)
            break;

    send_to_char(formatf("{WPattern #%d{X\n\r", index), ch);
    send_to_char(formatf("  Weight:   {C%d{X\n\r", pattern->weight), ch);
    {
        char name_escaped[MSL];
        char templ_escaped[MSL];
        rsg_escape_template_for_display(pattern->name, name_escaped, sizeof(name_escaped));
        rsg_escape_template_for_display(pattern->template, templ_escaped, sizeof(templ_escaped));
        send_to_char(formatf("  Name:     {C%s{X\n\r", name_escaped), ch);
        send_to_char(formatf("  Template: {C%s{X\n\r", templ_escaped), ch);
    }
    send_to_char(formatf("  Classes:  {C%d{X\n\r", pattern->classes), ch);
    for (i = 0; i < pattern->classes; i++)
        send_to_char(formatf("    - %s\n\r", pattern->c_list[i]->name), ch);

    return false;
}

RSGEDIT(rsgedit_pattern_delete)
{
    RANDOM_STRING *rsg;
    RANDOM_PATTERN *pattern;
    RANDOM_PATTERN *prev = NULL;
    int index;
    int position = 1;

    EDIT_RSG(ch, rsg);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: pattern delete <index|name>\n\r", ch);
        return false;
    }

    if (is_number(argument))
        index = atoi(argument);
    else
        index = -1;

    for (pattern = rsg->p_head; pattern; pattern = pattern->next, position++) {
        if ((index > 0 && position == index) || (index <= 0 && !str_cmp(pattern->name, argument)))
            break;
        prev = pattern;
    }

    if (!pattern) {
        send_to_char("No pattern by that index or name.\n\r", ch);
        return false;
    }

    rsgedit_record(rsg, ch, "pattern",
        formatf("%s: %s", pattern->name ? pattern->name : "", pattern->template ? pattern->template : ""),
        "(deleted)");

    if (prev)
        prev->next = pattern->next;
    else
        rsg->p_head = pattern->next;

    if (rsg->p_tail == pattern)
        rsg->p_tail = prev;

    rsg->patterns = UMAX(0, rsg->patterns - 1);
    rsg_free_pattern(pattern);

    if (!rsg_mark_dirty_and_save())
        send_to_char("Pattern deleted, but failed to save rsg.json.\n\r", ch);
    else
        send_to_char("Pattern deleted.\n\r", ch);

    return true;
}

RSGEDIT(rsgedit_pattern_help)
{
    send_to_char("Pattern commands:\n\r", ch);
    send_to_char("  pattern list\n\r", ch);
    send_to_char("  pattern create <weight> <name> <template>\n\r", ch);
    send_to_char("  pattern edit <index|name> <field> <value>\n\r", ch);
    send_to_char("    fields: weight | name | template\n\r", ch);
    send_to_char("    Placeholders use {{class_name}}; '{{' emits a literal '{'.\n\r", ch);
    send_to_char("  pattern show <index|name>\n\r", ch);
    send_to_char("  pattern delete <index|name>\n\r", ch);
    return false;
}

RSGEDIT(rsgedit_class_list)
{
    RANDOM_STRING *rsg;
    RANDOM_CLASS *cls;

    EDIT_RSG(ch, rsg);

    if (!rsg->c_head) {
        send_to_char("No classes defined.\n\r", ch);
        return false;
    }

    send_to_char("{WUID   Entries  Name{X\n\r", ch);
    send_to_char("{D----- -------  ------------------------------{X\n\r", ch);
    for (cls = rsg->c_head; cls; cls = cls->next)
        send_to_char(formatf("{W%-5ld %-7ld  {x%s{X\n\r", cls->uid, cls->entries, cls->name), ch);

    return false;
}

RSGEDIT(rsgedit_class_create)
{
    RANDOM_STRING *rsg;
    RANDOM_CLASS *cls;
    long max_uid = 0;
    RANDOM_CLASS *scan;

    EDIT_RSG(ch, rsg);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: class create <name>\n\r", ch);
        return false;
    }

    if (rsg_class_find_by_name(rsg, argument)) {
        send_to_char("A class with that name already exists.\n\r", ch);
        return false;
    }

    for (scan = rsg->c_head; scan; scan = scan->next)
        if (scan->uid > max_uid)
            max_uid = scan->uid;

    cls = alloc_mem(sizeof(*cls));
    memset(cls, 0, sizeof(*cls));
    cls->uid = max_uid + 1;
    cls->name = str_dup(argument);

    rsg_append_class(rsg, cls);
    rsg_rebuild_all_pattern_refs(rsg);
    rsgedit_record(rsg, ch, "class", "", cls->name);

    if (!rsg_mark_dirty_and_save())
        send_to_char("Class created, but failed to save rsg.json.\n\r", ch);
    else
        send_to_char("Class created.\n\r", ch);

    return true;
}

RSGEDIT(rsgedit_class_show)
{
    RANDOM_STRING *rsg;
    RANDOM_CLASS *cls;
    RANDOM_STRING_ENTRY *entry;
    int index = 1;

    EDIT_RSG(ch, rsg);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: class show <uid|name>\n\r", ch);
        return false;
    }

    cls = rsg_class_resolve(rsg, argument);
    if (!cls) {
        send_to_char("No class by that uid or name.\n\r", ch);
        return false;
    }

    send_to_char(formatf("{WClass: {C%s{W (UID %ld){X\n\r", cls->name, cls->uid), ch);
    send_to_char("{WIdx  Weight  Entry{X\n\r", ch);
    send_to_char("{D---  ------  ----------------------------------------------{X\n\r", ch);
    for (entry = cls->e_head; entry; entry = entry->next, index++)
        send_to_char(formatf("{W%3d  %6d  {x%s{X\n\r", index, entry->weight, entry->str), ch);

    if (index == 1)
        send_to_char("(No entries)\n\r", ch);

    return false;
}

RSGEDIT(rsgedit_class_delete)
{
    RANDOM_STRING *rsg;
    RANDOM_CLASS *cls;
    RANDOM_CLASS *prev = NULL;

    EDIT_RSG(ch, rsg);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: class delete <uid|name>\n\r", ch);
        return false;
    }

    cls = rsg_class_resolve(rsg, argument);
    if (!cls) {
        send_to_char("No class by that uid or name.\n\r", ch);
        return false;
    }

    rsgedit_record(rsg, ch, "class", cls->name, "(deleted)");

    if (rsg->c_head == cls)
        rsg->c_head = cls->next;
    else {
        for (prev = rsg->c_head; prev && prev->next != cls; prev = prev->next)
            ;
        if (prev)
            prev->next = cls->next;
    }

    if (rsg->c_tail == cls)
        rsg->c_tail = prev;

    rsg->classes = UMAX(0, rsg->classes - 1);
    rsg_free_class(cls);
    rsg_rebuild_all_pattern_refs(rsg);

    if (!rsg_mark_dirty_and_save())
        send_to_char("Class deleted, but failed to save rsg.json.\n\r", ch);
    else
        send_to_char("Class deleted.\n\r", ch);

    return true;
}

RSGEDIT(rsgedit_class_add)
{
    RANDOM_STRING *rsg;
    RANDOM_CLASS *cls;
    RANDOM_STRING_ENTRY *entry;
    char class_arg[MIL];
    char weight_arg[MIL];
    int weight;

    EDIT_RSG(ch, rsg);

    argument = one_argument(argument, class_arg);
    argument = one_argument(argument, weight_arg);

    if (IS_NULLSTR(class_arg) || IS_NULLSTR(weight_arg) || !is_number(weight_arg) || IS_NULLSTR(argument)) {
        send_to_char("Syntax: class add <class uid|name> <weight> <text>\n\r", ch);
        return false;
    }

    cls = rsg_class_resolve(rsg, class_arg);
    if (!cls) {
        send_to_char("No class by that uid or name.\n\r", ch);
        return false;
    }

    weight = atoi(weight_arg);
    if (weight < 1)
        weight = 1;

    entry = alloc_mem(sizeof(*entry));
    memset(entry, 0, sizeof(*entry));
    entry->weight = weight;
    entry->str = str_dup(argument);
    rsg_append_entry(cls, entry);
    rsgedit_record(rsg, ch, "class entry", "",
        formatf("%s (%d): %s", cls->name, entry->weight, entry->str));

    if (!rsg_mark_dirty_and_save())
        send_to_char("Entry added, but failed to save rsg.json.\n\r", ch);
    else
        send_to_char("Class entry added.\n\r", ch);

    return true;
}

RSGEDIT(rsgedit_class_edit)
{
    RANDOM_STRING *rsg;
    RANDOM_CLASS *cls;
    RANDOM_STRING_ENTRY *entry;
    char class_arg[MIL];
    char index_arg[MIL];
    char weight_arg[MIL];
    char old_text[MSL];
    char new_text[MSL];
    int index;
    int weight;

    EDIT_RSG(ch, rsg);

    argument = one_argument(argument, class_arg);
    argument = one_argument(argument, index_arg);
    argument = one_argument(argument, weight_arg);

    if (IS_NULLSTR(class_arg) || IS_NULLSTR(index_arg) || IS_NULLSTR(weight_arg) ||
        !is_number(index_arg) || !is_number(weight_arg) || IS_NULLSTR(argument)) {
        send_to_char("Syntax: class edit <class uid|name> <entry index> <weight> <text>\n\r", ch);
        return false;
    }

    cls = rsg_class_resolve(rsg, class_arg);
    if (!cls) {
        send_to_char("No class by that uid or name.\n\r", ch);
        return false;
    }

    index = atoi(index_arg);
    entry = rsg_class_entry_by_index(cls, index);
    if (!entry) {
        send_to_char("No entry at that index.\n\r", ch);
        return false;
    }

    weight = atoi(weight_arg);
    if (weight < 1)
        weight = 1;

    snprintf(old_text, sizeof(old_text), "%s[%d] (%d): %s",
        cls->name, index, entry->weight, IS_NULLSTR(entry->str) ? "" : entry->str);
    snprintf(new_text, sizeof(new_text), "%s[%d] (%d): %s",
        cls->name, index, weight, argument);
    rsgedit_record(rsg, ch, "class entry", old_text, new_text);

    entry->weight = weight;
    free_string(entry->str);
    entry->str = str_dup(argument);

    if (!rsg_mark_dirty_and_save())
        send_to_char("Entry updated, but failed to save rsg.json.\n\r", ch);
    else
        send_to_char("Class entry updated.\n\r", ch);

    return true;
}

RSGEDIT(rsgedit_class_remove)
{
    RANDOM_STRING *rsg;
    RANDOM_CLASS *cls;
    RANDOM_STRING_ENTRY *entry;
    RANDOM_STRING_ENTRY *prev = NULL;
    char class_arg[MIL];
    char index_arg[MIL];
    char old_text[MSL];
    int index;
    int pos = 1;

    EDIT_RSG(ch, rsg);

    argument = one_argument(argument, class_arg);
    argument = one_argument(argument, index_arg);

    if (IS_NULLSTR(class_arg) || IS_NULLSTR(index_arg) || !is_number(index_arg)) {
        send_to_char("Syntax: class remove <class uid|name> <entry index>\n\r", ch);
        return false;
    }

    cls = rsg_class_resolve(rsg, class_arg);
    if (!cls) {
        send_to_char("No class by that uid or name.\n\r", ch);
        return false;
    }

    index = atoi(index_arg);
    for (entry = cls->e_head; entry; entry = entry->next, pos++) {
        if (pos == index)
            break;
        prev = entry;
    }

    if (!entry) {
        send_to_char("No entry at that index.\n\r", ch);
        return false;
    }

    snprintf(old_text, sizeof(old_text), "%s[%d] (%d): %s",
        cls->name, index, entry->weight, IS_NULLSTR(entry->str) ? "" : entry->str);
    rsgedit_record(rsg, ch, "class entry", old_text, "(deleted)");

    if (prev)
        prev->next = entry->next;
    else
        cls->e_head = entry->next;

    if (cls->e_tail == entry)
        cls->e_tail = prev;

    cls->entries = UMAX(0, cls->entries - 1);
    rsg_free_entry(entry);

    if (!rsg_mark_dirty_and_save())
        send_to_char("Entry removed, but failed to save rsg.json.\n\r", ch);
    else
        send_to_char("Class entry removed.\n\r", ch);

    return true;
}

RSGEDIT(rsgedit_class_help)
{
    send_to_char("Class commands:\n\r", ch);
    send_to_char("  class list\n\r", ch);
    send_to_char("  class create <name>\n\r", ch);
    send_to_char("  class show <uid|name>\n\r", ch);
    send_to_char("  class delete <uid|name>\n\r", ch);
    send_to_char("  class add <uid|name> <weight> <text>\n\r", ch);
    send_to_char("  class edit <uid|name> <index> <weight> <text>\n\r", ch);
    send_to_char("  class remove <uid|name> <index>\n\r", ch);
    return false;
}
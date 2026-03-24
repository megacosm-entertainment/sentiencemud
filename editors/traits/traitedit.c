/***************************************************************************
 *  traitedit.c - OLC Trait Definition Editor                              *
 *                                                                         *
 *  Allows in-game editing of trait definitions stored in                   *
 *  data/traits/traits.json. Follows the cmdedit pattern.                  *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework.                          *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../recycle.h"
#include "../../traits.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"
#include "../../io/json/json_olc.h"

/* Forward declarations for save function (in traits.c) */
extern bool save_trait_definitions(void);

/***************************************************************************
 * History Helpers                                                         *
 ***************************************************************************/

static OLC_CHANGE_HISTORY *traitedit_get_history(void *pEdit)
{
    TRAIT_DEF *def = (TRAIT_DEF *)pEdit;
    return def ? (OLC_CHANGE_HISTORY *)def->olc_history : NULL;
}

static OLC_CHANGE_HISTORY *traitedit_ensure_history(TRAIT_DEF *def)
{
    if (!def) return NULL;
    if (!def->olc_history)
        def->olc_history = olc_history_load(OLC_HIST_TRAIT, def->id);
    if (!def->olc_history)
        def->olc_history = olc_history_new();
    return (OLC_CHANGE_HISTORY *)def->olc_history;
}

static void traitedit_record(TRAIT_DEF *def, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    olc_history_record(traitedit_ensure_history(def), ch,
        field, old_val, new_val);
    olc_history_mark_dirty(OLC_HIST_TRAIT, def->id,
        (OLC_CHANGE_HISTORY *)def->olc_history);
}

/** Generic callback wrapper for olc_cmd_* helpers. */
static void traitedit_record_cb(void *ctx, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    traitedit_record((TRAIT_DEF *)ctx, ch, field, old_val, new_val);
}

/***************************************************************************
 * Trait Editor Command Table                                              *
 ***************************************************************************/

const struct olc_cmd_type traitedit_table[] =
{
    { "?",              show_help           },
    { "category",       traitedit_category  },
    { "commands",       show_commands       },
    { "create",         traitedit_create    },
    { "default",        traitedit_default   },
    { "delete",         traitedit_delete    },
    { "description",    traitedit_description },
    { "list",           traitedit_list      },
    { "name",           traitedit_name      },
    { "save",           traitedit_save      },
    { "show",           traitedit_show      },
    { "type",           traitedit_type      },
    { NULL,             0                   }
};


/***************************************************************************
 * Editor Definition (Unified Framework)                                   *
 ***************************************************************************/

static const OLC_EDITOR_DEF traitedit_def = {
    .name           = "TraitEdit",
    .editor_type    = ED_TRAIT,
    .cmd_table      = traitedit_table,
    .show_fn        = traitedit_show,
    .tabs           = { .count = 0 },           /* No tabs for trait editor */
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = true,
    .get_history_fn = traitedit_get_history,
};


/***************************************************************************
 * Editor Entry Point                                                      *
 ***************************************************************************/

/**
 * do_traitedit - Enter the trait definition editor
 *
 * Usage: traitedit <trait id>
 *        traitedit list
 *        traitedit create <id>
 *
 * @param ch        Character entering the editor
 * @param argument  Trait ID or name to edit, "list", or "create <id>"
 */
void do_traitedit(CHAR_DATA *ch, char *argument)
{
    TRAIT_DEF *def;
    char arg1[MAX_INPUT_LENGTH];

    if (IS_NPC(ch))
        return;

    /* Permission check at entry */
    if (!olc_editor_check_perm(ch, &traitedit_def, NULL)) {
        send_to_char("You don't have permission to edit traits.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  traitedit <trait id or name>\n\r", ch);
        send_to_char("         traitedit list\n\r", ch);
        send_to_char("         traitedit create <id>\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "list")) {
        traitedit_list(ch, argument);
        return;
    }

    if (!str_cmp(arg1, "create")) {
        if (traitedit_create(ch, argument))
            olc_editor_enter(ch, &traitedit_def, ch->desc->pEdit, true);
        return;
    }

    def = trait_def_lookup_name(arg1);
    if (!def) {
        send_to_char("No trait definition by that ID or name.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &traitedit_def, def, true);
}


/***************************************************************************
 * Editor Interpreter (Framework)                                          *
 ***************************************************************************/

/**
 * traitedit - Interpreter loop for the trait definition editor
 *
 * All boilerplate (done, show, command dispatch, interpret fallback)
 * is handled by the framework.
 *
 * @param ch        Character in the editor
 * @param argument  Command typed by the character
 */
void traitedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &traitedit_def);
}


/***************************************************************************
 * Display                                                                 *
 ***************************************************************************/

/**
 * traitedit_show - Display current trait definition data
 *
 * @param ch        Character viewing
 * @param argument  Unused
 * @return          false (no data changed)
 */
TRAITEDIT(traitedit_show)
{
    TRAIT_DEF *def;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&traitedit_def);
    OLC_LAYOUT_CTX *ctx;

    EDIT_TRAIT(ch, def);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "TraitEdit", def->name,
        formatf("#%d", def->index), &traitedit_def);

    olc_display_string(ctx, theme, "ID:", NULL, def->id);
    olc_display_number(ctx, theme, "Index:", NULL, def->index);
    olc_display_string(ctx, theme, "Name:", "name", def->name);
    olc_display_string(ctx, theme, "Category:", "category", def->category);

    {
        const char *type_str = "???";
        switch (def->type) {
            case TRAIT_BOOLEAN: type_str = "boolean"; break;
            case TRAIT_INTEGER: type_str = "integer"; break;
            case TRAIT_STRING:  type_str = "string";  break;
        }
        olc_display_string(ctx, theme, "Type:", "type", type_str);
    }

    switch (def->type) {
        case TRAIT_BOOLEAN:
            olc_display_bool(ctx, theme, "Default:", "default", def->default_bool);
            break;
        case TRAIT_INTEGER:
            olc_display_number(ctx, theme, "Default:", "default", def->default_int);
            break;
        case TRAIT_STRING:
            olc_display_string(ctx, theme, "Default:", "default", def->default_string);
            break;
    }

    olc_display_text(ctx, theme, "Description:", "description", def->description);

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}


/***************************************************************************
 * Editor Commands                                                         *
 ***************************************************************************/

/**
 * traitedit_name - Set the trait display name
 *
 * @param ch        Character editing
 * @param argument  New name string
 * @return          true if changed
 */
TRAITEDIT(traitedit_name)
{
    TRAIT_DEF *def;
    EDIT_TRAIT(ch, def);
    return olc_cmd_string(ch, argument, "Name", NULL, &def->name,
        OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, def, traitedit_record_cb);
}


/**
 * traitedit_category - Set the trait category
 *
 * @param ch        Character editing
 * @param argument  Category string
 * @return          true if changed
 */
TRAITEDIT(traitedit_category)
{
    TRAIT_DEF *def;
    EDIT_TRAIT(ch, def);
    return olc_cmd_string(ch, argument, "Category", NULL, &def->category,
        OLC_STR_DEFAULT, def, traitedit_record_cb);
}


/**
 * traitedit_description - Edit the trait description (opens string editor)
 *
 * @param ch        Character editing
 * @param argument  Unused
 * @return          true (opens string editor)
 */
TRAITEDIT(traitedit_description)
{
    TRAIT_DEF *def;
    EDIT_TRAIT(ch, def);
    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &def->description, def, traitedit_record_cb);
}


/**
 * traitedit_type - Set the trait value type
 *
 * Warning: changing the type resets the default value.
 *
 * @param ch        Character editing
 * @param argument  "boolean", "integer", or "string"
 * @return          true if changed
 */
TRAITEDIT(traitedit_type)
{
    TRAIT_DEF *def;
    EDIT_TRAIT(ch, def);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  type <boolean|integer|string>\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "boolean") || !str_cmp(argument, "bool")) {
        traitedit_record(def, ch, "type", "(changed)", "boolean");
        def->type = TRAIT_BOOLEAN;
        def->default_bool = false;
        def->default_int = 0;
        free_string(def->default_string);
        def->default_string = NULL;
    } else if (!str_cmp(argument, "integer") || !str_cmp(argument, "int")) {
        traitedit_record(def, ch, "type", "(changed)", "integer");
        def->type = TRAIT_INTEGER;
        def->default_bool = false;
        def->default_int = 0;
        free_string(def->default_string);
        def->default_string = NULL;
    } else if (!str_cmp(argument, "string") || !str_cmp(argument, "str")) {
        traitedit_record(def, ch, "type", "(changed)", "string");
        def->type = TRAIT_STRING;
        def->default_bool = false;
        def->default_int = 0;
        free_string(def->default_string);
        def->default_string = NULL;
    } else {
        send_to_char("Invalid type. Use: boolean, integer, or string.\n\r", ch);
        return false;
    }

    send_to_char("Trait type set. Default value has been reset.\n\r", ch);
    return true;
}


/**
 * traitedit_default - Set the default value for the trait
 *
 * @param ch        Character editing
 * @param argument  Default value appropriate to the trait type
 * @return          true if changed
 */
TRAITEDIT(traitedit_default)
{
    TRAIT_DEF *def;
    EDIT_TRAIT(ch, def);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  default <value>\n\r", ch);
        return false;
    }

    switch (def->type) {
        case TRAIT_BOOLEAN:
            if (!str_cmp(argument, "true") || !str_cmp(argument, "yes") || !str_cmp(argument, "1")) {
                traitedit_record(def, ch, "default", def->default_bool ? "true" : "false", "true");
                def->default_bool = true;
            } else if (!str_cmp(argument, "false") || !str_cmp(argument, "no") || !str_cmp(argument, "0")) {
                traitedit_record(def, ch, "default", def->default_bool ? "true" : "false", "false");
                def->default_bool = false;
            } else {
                send_to_char("Boolean default: use true/false, yes/no, or 1/0.\n\r", ch);
                return false;
            }
            break;
        case TRAIT_INTEGER:
            if (!is_number(argument)) {
                send_to_char("Integer default: provide a numeric value.\n\r", ch);
                return false;
            }
            traitedit_record(def, ch, "default", formatf("%d", def->default_int), argument);
            def->default_int = atoi(argument);
            break;
        case TRAIT_STRING:
            traitedit_record(def, ch, "default", def->default_string ? def->default_string : "", argument);
            free_string(def->default_string);
            def->default_string = str_dup(argument);
            break;
    }

    send_to_char("Default value set.\n\r", ch);
    return true;
}


/**
 * traitedit_create - Create a new trait definition
 *
 * @param ch        Character creating
 * @param argument  Unique trait ID
 * @return          true if created
 */
TRAITEDIT(traitedit_create)
{
    TRAIT_DEF *def, *last;

    if (argument[0] == '\0') {
        send_to_char("Syntax:  create <trait_id>\n\r", ch);
        return false;
    }

    /* Check for duplicates */
    if (trait_def_lookup(argument)) {
        send_to_char("A trait with that ID already exists.\n\r", ch);
        return false;
    }

    /* Validate ID (lowercase, underscores, no spaces) */
    for (const char *p = argument; *p; p++) {
        if (!islower(*p) && *p != '_' && !isdigit(*p)) {
            send_to_char("Trait ID must be lowercase with underscores only.\n\r", ch);
            return false;
        }
    }

    def = (TRAIT_DEF *)alloc_perm(sizeof(TRAIT_DEF));
    memset(def, 0, sizeof(TRAIT_DEF));
    def->valid = true;
    def->index = trait_def_count;
    def->id = str_dup(argument);
    def->name = str_dup(argument);
    def->description = str_dup("");
    def->category = str_dup("");
    def->type = TRAIT_BOOLEAN;
    def->default_bool = false;

    /* Append to linked list */
    if (!trait_def_list) {
        trait_def_list = def;
    } else {
        for (last = trait_def_list; last->next; last = last->next)
            ;
        last->next = def;
    }
    def->next = NULL;
    trait_def_count++;

    ch->desc->pEdit = (void *)def;

    send_to_char(formatf("Trait '%s' created.\n\r", argument), ch);
    return true;
}


/**
 * traitedit_delete - Delete a trait definition
 *
 * @param ch        Character deleting
 * @param argument  "confirm" to actually delete
 * @return          true if deleted
 */
TRAITEDIT(traitedit_delete)
{
    TRAIT_DEF *def, *prev;
    EDIT_TRAIT(ch, def);

    if (str_cmp(argument, "confirm")) {
        send_to_char("Type 'delete confirm' to permanently delete this trait.\n\r", ch);
        send_to_char("{RWARNING: This does NOT update existing races that use this trait!{x\n\r", ch);
        return false;
    }

    /* Remove from linked list */
    if (trait_def_list == def) {
        trait_def_list = def->next;
    } else {
        for (prev = trait_def_list; prev && prev->next != def; prev = prev->next)
            ;
        if (prev)
            prev->next = def->next;
    }

    def->valid = false;
    trait_def_count--;

    /* Reindex remaining traits */
    {
        int idx = 0;
        TRAIT_DEF *t;
        for (t = trait_def_list; t; t = t->next)
            t->index = idx++;
    }

    send_to_char("Trait deleted. Use 'save' from another trait to persist changes.\n\r", ch);
    edit_done(ch);
    return true;
}


/**
 * traitedit_save - Save all trait definitions to traits.json
 *
 * @param ch        Character editing
 * @param argument  Unused
 * @return          true if saved
 */
TRAITEDIT(traitedit_save)
{
    TRAIT_DEF *def;
    EDIT_TRAIT(ch, def);

    if (save_trait_definitions()) {
        olc_history_flush(OLC_HIST_TRAIT, def->id, def->olc_history);
        send_to_char("Trait definitions saved.\n\r", ch);
        return true;
    } else {
        send_to_char("Error saving trait definitions!\n\r", ch);
        return false;
    }
}


/**
 * traitedit_list - List all trait definitions
 *
 * @param ch        Character viewing
 * @param argument  Optional filter string
 * @return          false (display only)
 */
TRAITEDIT(traitedit_list)
{
    TRAIT_DEF *def;
    BUFFER *buffer;
    bool append_ok = true;
    int count = 0;

    buffer = new_buf();
    if (!add_buf(buffer, "{R  #  ID                            Type     Category       Default{x\n\r")
        || !add_buf(buffer, "{D --- ------------------------------ -------- -------------- -------{x\n\r")) {
        send_to_char("Trait list output exceeded buffer limits.\n\r", ch);
        free_buf(buffer);
        return false;
    }

    for (def = trait_def_list; def; def = def->next) {
        const char *type_str = "???";
        char default_str[64];

        if (argument[0] != '\0' && str_prefix(argument, def->id)
            && str_prefix(argument, def->category))
            continue;

        switch (def->type) {
            case TRAIT_BOOLEAN: type_str = "boolean"; break;
            case TRAIT_INTEGER: type_str = "integer"; break;
            case TRAIT_STRING:  type_str = "string";  break;
        }

        switch (def->type) {
            case TRAIT_BOOLEAN:
                sprintf(default_str, "%s", def->default_bool ? "true" : "false");
                break;
            case TRAIT_INTEGER:
                sprintf(default_str, "%d", def->default_int);
                break;
            case TRAIT_STRING:
                sprintf(default_str, "%s", def->default_string ? def->default_string : "(null)");
                break;
        }

        if (!add_buf(buffer, formatf(" {C%3d{x %-30s %-8s %-14s %s\n\r",
            def->index, def->id, type_str,
            IS_NULLSTR(def->category) ? "-" : def->category,
            default_str))) {
            append_ok = false;
            break;
        }
        count++;
    }

    if (append_ok && !add_buf(buffer, formatf("\n\r{x%d trait%s listed.\n\r", count, count == 1 ? "" : "s")))
        append_ok = false;

    if (!append_ok) {
        send_to_char("Trait list output exceeded buffer limits.\n\r", ch);
        free_buf(buffer);
        return false;
    }

    page_to_char(buffer->string, ch);
    free_buf(buffer);
    return false;
}

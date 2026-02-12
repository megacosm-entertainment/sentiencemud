/***************************************************************************
 *  traitedit.c - OLC Trait Definition Editor                              *
 *                                                                         *
 *  Allows in-game editing of trait definitions stored in                   *
 *  data/traits/traits.json. Follows the cmdedit pattern.                  *
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

/* Forward declarations for save function (in traits.c) */
extern bool save_trait_definitions(void);

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
            ch->desc->editor = ED_TRAIT;
        return;
    }

    def = trait_def_lookup_name(arg1);
    if (!def) {
        send_to_char("No trait definition by that ID or name.\n\r", ch);
        return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    olc_set_editor(ch, ED_TRAIT, def);
    traitedit_show(ch, "");
}


/***************************************************************************
 * Editor Interpreter                                                      *
 ***************************************************************************/

/**
 * traitedit - Interpreter loop for the trait definition editor
 *
 * Dispatches typed commands to the trait editor command table.
 *
 * @param ch        Character in the editor
 * @param argument  Command typed by the character
 */
void traitedit(CHAR_DATA *ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_STRING_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (!str_cmp(command, "done")) {
        edit_done(ch);
        return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;

    if (command[0] == '\0') {
        traitedit_show(ch, argument);
        return;
    }

    for (cmd = 0; traitedit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, traitedit_table[cmd].name)) {
            (*traitedit_table[cmd].olc_fun)(ch, argument);
            return;
        }
    }

    interpret(ch, arg);
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
    BUFFER *buffer;

    EDIT_TRAIT(ch, def);
    buffer = new_buf();

    add_buf(buffer, formatf("{R=== Trait Editor: %s ==={x\n\r\n\r", def->id));

    add_buf(buffer, formatf("{CID:           {x%s\n\r", def->id));
    add_buf(buffer, formatf("{CIndex:        {x%d\n\r", def->index));
    add_buf(buffer, formatf("{CName:         {W%s{x\n\r", def->name));
    add_buf(buffer, formatf("{CCategory:     {x%s\n\r",
        IS_NULLSTR(def->category) ? "(none)" : def->category));

    {
        const char *type_str = "???";
        switch (def->type) {
            case TRAIT_BOOLEAN: type_str = "boolean"; break;
            case TRAIT_INTEGER: type_str = "integer"; break;
            case TRAIT_STRING:  type_str = "string";  break;
        }
        add_buf(buffer, formatf("{CType:         {x%s\n\r", type_str));
    }

    switch (def->type) {
        case TRAIT_BOOLEAN:
            add_buf(buffer, formatf("{CDefault:      {x%s\n\r",
                def->default_bool ? "true" : "false"));
            break;
        case TRAIT_INTEGER:
            add_buf(buffer, formatf("{CDefault:      {x%d\n\r", def->default_int));
            break;
        case TRAIT_STRING:
            add_buf(buffer, formatf("{CDefault:      {x%s\n\r",
                def->default_string ? def->default_string : "(null)"));
            break;
    }

    add_buf(buffer, formatf("\n\r{CDescription:{x\n\r%s\n\r",
        IS_NULLSTR(def->description) ? "   (none)" : def->description));

    page_to_char(buffer->string, ch);
    free_buf(buffer);
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

    if (argument[0] == '\0') {
        send_to_char("Syntax:  name <name>\n\r", ch);
        return false;
    }

    free_string(def->name);
    def->name = str_dup(argument);
    send_to_char("Trait name set.\n\r", ch);
    return true;
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

    if (argument[0] == '\0') {
        send_to_char("Syntax:  category <category>\n\r", ch);
        return false;
    }

    free_string(def->category);
    def->category = str_dup(argument);
    send_to_char("Category set.\n\r", ch);
    return true;
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

    if (argument[0] == '\0') {
        string_append(ch, &def->description);
        return true;
    }

    send_to_char("Syntax:  description    (opens string editor)\n\r", ch);
    return false;
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
        def->type = TRAIT_BOOLEAN;
        def->default_bool = false;
        def->default_int = 0;
        free_string(def->default_string);
        def->default_string = NULL;
    } else if (!str_cmp(argument, "integer") || !str_cmp(argument, "int")) {
        def->type = TRAIT_INTEGER;
        def->default_bool = false;
        def->default_int = 0;
        free_string(def->default_string);
        def->default_string = NULL;
    } else if (!str_cmp(argument, "string") || !str_cmp(argument, "str")) {
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
            if (!str_cmp(argument, "true") || !str_cmp(argument, "yes") || !str_cmp(argument, "1"))
                def->default_bool = true;
            else if (!str_cmp(argument, "false") || !str_cmp(argument, "no") || !str_cmp(argument, "0"))
                def->default_bool = false;
            else {
                send_to_char("Boolean default: use true/false, yes/no, or 1/0.\n\r", ch);
                return false;
            }
            break;
        case TRAIT_INTEGER:
            if (!is_number(argument)) {
                send_to_char("Integer default: provide a numeric value.\n\r", ch);
                return false;
            }
            def->default_int = atoi(argument);
            break;
        case TRAIT_STRING:
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

    ch->pcdata->immortal->last_olc_command = current_time;
    olc_set_editor(ch, ED_TRAIT, def);

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
    if (save_trait_definitions()) {
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
    int count = 0;

    buffer = new_buf();
    add_buf(buffer, "{R  #  ID                            Type     Category       Default{x\n\r");
    add_buf(buffer, "{D --- ------------------------------ -------- -------------- -------{x\n\r");

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

        add_buf(buffer, formatf(" {C%3d{x %-30s %-8s %-14s %s\n\r",
            def->index, def->id, type_str,
            IS_NULLSTR(def->category) ? "-" : def->category,
            default_str));
        count++;
    }

    add_buf(buffer, formatf("\n\r{x%d trait%s listed.\n\r", count, count == 1 ? "" : "s"));
    page_to_char(buffer->string, ch);
    free_buf(buffer);
    return false;
}

/***************************************************************************
 *  cmdedit.c — OLC Command Editor                                        *
 *                                                                         *
 *  In-game editor for CMD_DATA definitions. Allows immortals to create,  *
 *  modify, and manage game command properties including function binding, *
 *  permissions, logging, and metadata.                                    *
 *                                                                         *
 *  Also contains non-editor utility functions: get_cmd_data,              *
 *  load_commands, save_commands, do_cmdlist.                              *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework.                          *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <math.h>
#include "../../strings.h"
#include "../../merc.h"
#include "../../interp.h"
#include "../../db.h"
#include "../../recycle.h"
#include "../../tables.h"
#include "../../bootstrap/bootstrap_internal.h"
#include "../../olc.h"
#include "../../olc_save.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../../io/json/json_commands.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table);

bool commands_changed = false;

static CMD_DATA *cmdedit_find_command_exact(const char *name)
{
    ITERATOR it;
    CMD_DATA *command;

    if (IS_NULLSTR(name))
        return NULL;

    iterator_start(&it, commands_list);
    while ((command = (CMD_DATA *)iterator_nextdata(&it))) {
        if (!str_cmp(command->name, name))
            break;
    }
    iterator_stop(&it);

    return command;
}

static bool cmdedit_delete_command(CHAR_DATA *ch, CMD_DATA *command)
{
    if (!command)
        return false;

    if (!str_cmp(command->name, "cmdedit")) {
        send_to_char("You cannot delete cmdedit.\n\r", ch);
        return false;
    }

    list_remlink(commands_list, command, true);
    commands_changed = true;

    send_to_char("Command deleted. Use 'asave changed' to persist changes.\n\r", ch);
    return true;
}

static long cmdedit_addl_types_mask(void)
{
    long mask = 0;

    for (int i = 0; command_addl_types[i].name != NULL; i++) {
        if (command_addl_types[i].settable)
            mask |= command_addl_types[i].bit;
    }

    return mask;
}

static long cmdedit_type_to_addl_flag(long cmd_type)
{
    const char *type_name;
    long addl_flag;

    type_name = flag_name(command_types, cmd_type);
    if (IS_NULLSTR(type_name))
        return CMD_TYPE_NONE;

    addl_flag = flag_value(command_addl_types, (char *)type_name);
    if (addl_flag == NO_FLAG)
        return CMD_TYPE_NONE;

    return addl_flag;
}

static bool cmdedit_normalize_command(CMD_DATA *command)
{
    long mask;
    long normalized;

    if (!command)
        return false;

    mask = cmdedit_addl_types_mask();
    normalized = command->addl_types;

    if (normalized < 0)
        normalized = 0;

    normalized &= mask;

    if (normalized == 0)
        normalized = cmdedit_type_to_addl_flag(command->type) & mask;

    if (normalized == 0)
        normalized = CMD_TYPE_NONE & mask;

    if (normalized != command->addl_types) {
        command->addl_types = normalized;
        return true;
    }

    return false;
}

/***************************************************************************
 * Change Tracking                                                         *
 ***************************************************************************/

/**
 * cmdedit_mark_changed - Mark commands as needing save
 *
 * Sets the global commands_changed flag so the command list
 * is saved on the next area save cycle (asave changed).
 *
 * @param ch     Character who made the change
 * @param pEdit  CMD_DATA being edited
 */
static void cmdedit_mark_changed(CHAR_DATA *ch, void *pEdit)
{
    (void)ch;
    (void)pEdit;
    commands_changed = true;
}

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type cmdedit_table[] =
{
    { "?",           show_help           },
    { "additional",  cmdedit_additional  },
    { "commands",    show_commands       },
    { "comments",    cmdedit_comments    },
    { "create",      cmdedit_create      },
    { "delete",      cmdedit_delete      },
    { "description", cmdedit_description },
    { "enabled",     cmdedit_enabled     },
    { "flags",       cmdedit_flags       },
    { "function",    cmdedit_function    },
    { "log",         cmdedit_log         },
    { "name",        cmdedit_name        },
    { "order",       cmdedit_order       },
    { "position",    cmdedit_position    },
    { "rank",        cmdedit_rank        },
    { "reason",      cmdedit_reason      },
    { "sethelp",     cmdedit_help        },
    { "show",        cmdedit_show        },
    { "summary",     cmdedit_summary     },
    { "type",        cmdedit_type        },
    { NULL,          0                   }
};

/***************************************************************************
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF cmdedit_def = {
    .name           = "CMDEdit",
    .editor_type    = ED_CMDEDIT,
    .cmd_table      = cmdedit_table,
    .show_fn        = cmdedit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_system,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_IMPLEMENTOR
    },
    .change_mode    = OLC_CHANGE_CUSTOM,
    .mark_changed_fn = cmdedit_mark_changed,
    .audit_changes  = false,
    .get_history_fn = NULL,
};

/***************************************************************************
 * Entry Point                                                             *
 ***************************************************************************/

/**
 * do_cmdedit - Enter the command editor
 *
 * Syntax:
 *   cmdedit <command name>  - Edit an existing command
 *   cmdedit <command> delete - Delete a command (requires confirmation)
 *   cmdedit create <name>   - Create a new command
 *   cmdedit delete <name>   - Legacy delete syntax (still supported)
 */
void do_cmdedit(CHAR_DATA *ch, char *argument)
{
    CMD_DATA *command;
    char arg1[MSL];
    char arg2[MSL];
    char arg3[MSL];
    char arg4[MSL];

    argument = one_argument(argument, arg1);

    if (IS_NPC(ch))
        return;

    if (!olc_editor_check_perm(ch, &cmdedit_def, NULL)) {
        send_to_char("CMDEdit: Insufficient security to edit commands.\n\r", ch);
        return;
    }

    if (arg1[0] == '\0') {
        send_to_char("CMDEdit: There is no default command to edit.\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "create")) {
        if (cmdedit_create(ch, argument))
            olc_editor_enter(ch, &cmdedit_def, ch->desc->pEdit, false);
        return;
    }

    if (!str_cmp(arg1, "delete")) {
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);
        one_argument(argument, arg4);

        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: cmdedit delete <command>\n\r", ch);
            send_to_char("        cmdedit delete <command> delete <command>\n\r", ch);
            return;
        }

        command = cmdedit_find_command_exact(arg2);
        if (!command) {
            send_to_char("No command by that exact name.\n\r", ch);
            return;
        }

        if (str_cmp(arg3, "delete") || str_cmp(arg4, arg2)) {
            printf_to_char(ch,
                "Type 'cmdedit delete %s delete %s' to confirm permanent deletion.\n\r",
                arg2,
                arg2);
            return;
        }

        (void)cmdedit_delete_command(ch, command);
        return;
    }

    argument = one_argument(argument, arg2);
    one_argument(argument, arg3);

    if (!str_cmp(arg2, "delete")) {
        command = cmdedit_find_command_exact(arg1);
        if (!command) {
            send_to_char("No command by that exact name.\n\r", ch);
            return;
        }

        if (str_cmp(arg3, arg1)) {
            printf_to_char(ch,
                "Type 'cmdedit %s delete %s' to confirm permanent deletion.\n\r",
                arg1,
                arg1);
            return;
        }

        (void)cmdedit_delete_command(ch, command);
        return;
    }

    command = get_cmd_data(arg1);
    if (!command) {
        send_to_char("No command by that name.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &cmdedit_def, command, false);
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * cmdedit - Command interpreter for the command editor
 */
void cmdedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &cmdedit_def);
}

/**
 * do_cmdshow - Show a command's details without entering the editor
 */
void do_cmdshow(CHAR_DATA *ch, char *argument)
{
    CMD_DATA *command;

    if (argument[0] == '\0') {
        send_to_char("Syntax:  cmdshow <command name>\n\r", ch);
        return;
    }

    if (!(command = get_cmd_data(argument))) {
        send_to_char("That command does not exist.\n\r", ch);
        return;
    }

    olc_show_item(ch, command, cmdedit_show, argument);
    return;
}

/***************************************************************************
 * Non-Editor Utility Functions                                            *
 ***************************************************************************/

CMD_DATA *get_cmd_data(char *name)
{
    ITERATOR it;
    CMD_DATA *command;
    iterator_start(&it, commands_list);
    while((command = (CMD_DATA *)iterator_nextdata(&it)))
    {
        if (!str_prefix(name, command->name))
            break;
    }
    iterator_stop(&it);

    return command;
}

/*
DO_FUN * do_func_lookup(char *name)
{ 
    for (int i = 0; do_func_table[i].name != NULL; i++)
    {
        if (!str_cmp(name, do_func_table[i].name))
            return do_func_table[i].func;
    }

    return NULL;
}

char do_func_name(DO_FUN *func)
{
    for (int i = 0; do_func_table[i].name != NULL; i++)
    {
        if (do_func_table[i].func == func)
            return do_func_table[i].name;
    }

    return NULL;
}

char do_func_display(DO_FUN *func)
{
    if ( !func ) return NULL;

    for (int i = 0; do_func_table[i].name != NULL; i++)
    {
        if (do_func_table[i].func == func)
            return do_func_table[i].name;
    }

    return "(invalid)";
}
*/

#define FUNC_LOOKUPS(f,t,n) \
t * f##_func_lookup(char *name) \
{ \
    for (int i = 0; f##_func_table[i].name != NULL; i++) \
    { \
        if (!str_cmp(name, f##_func_table[i].name)) \
            return f##_func_table[i].func; \
    } \
 \
    return NULL; \
} \
 \
char *f##_func_name(t *func) \
{ \
    for (int i = 0; f##_func_table[i].name != NULL; i++) \
    { \
        if (f##_func_table[i].func == func) \
            return f##_func_table[i].name; \
    } \
 \
    return NULL; \
} \
 \
char *f##_func_display(t *func) \
{ \
    if ( n ) return NULL; \
 \
    for (int i = 0; f##_func_table[i].name != NULL; i++) \
    { \
        if (f##_func_table[i].func == func) \
            return f##_func_table[i].name; \
    } \
 \
    return "(invalid)"; \
} \
 \

FUNC_LOOKUPS(do, DO_FUN,(!func))

// Create a function to load commands from a command file. If the command file does not exist, boostrap one from the cmd_table.

void save_command(FILE *fp, CMD_DATA *command)
{
    fprintf(fp, "#COMMAND %s~\n", command->name);
    fprintf(fp, "Enabled %d\n", command->enabled);
    fprintf(fp, "Function %s~\n", do_func_name(command->function));
    fprintf(fp, "Rank %d\n", command->rank);
    fprintf(fp, "Log %d\n", command->log);
    fprintf(fp, "Position %d\n", command->position);
    fprintf(fp, "Type %ld\n", command->type);
    fprintf(fp, "Addl_Types %ld\n", command->addl_types);
    fprintf(fp, "Flags %ld\n", command->command_flags);
    fprintf(fp, "Comments %s~\n", command->comments);
    fprintf(fp, "Description %s~\n", command->description);
    if (command->help_keywords != NULL && !IS_NULLSTR(command->help_keywords->string))
        fprintf(fp, "HelpKeywords %s~\n", command->help_keywords->string);
    if (!IS_NULLSTR(command->reason))
        fprintf(fp, "Reason %s~\n", command->reason);
    if (!IS_NULLSTR(command->summary))
        fprintf(fp, "Summary %s~\n", command->summary);
    fprintf(fp, "#-COMMAND\n");
}

void save_commands()
{
    json_save_commands(COMMANDS_JSON_FILE);
}

void insert_command(CMD_DATA *command)
{
        list_appendlink(commands_list, command);
}

static void delete_command(void *ptr)
{
    free_cmd((CMD_DATA *)ptr);
}

bool load_commands()
{
    commands_list = list_createx(false, NULL, delete_command);

    if (!IS_VALID(commands_list))
    {
        pbugf(LOG_INIT, "commands_list is not valid.");
        return false;
    }

    if (json_load_commands(COMMANDS_JSON_FILE))
    {
        ITERATOR it;
        CMD_DATA *command;

        iterator_start(&it, commands_list);
        while ((command = (CMD_DATA *)iterator_nextdata(&it)))
            cmdedit_normalize_command(command);
        iterator_stop(&it);

        return true;
    }

    log_string("commands.json not found, seeding from bootstrap_data...");

    if (create_commands_json() && json_load_commands(COMMANDS_JSON_FILE))
    {
        log_string("Seeded and loaded commands.json from bootstrap_data");
        return true;
    }

    perrf(LOG_INIT, "Failed to load or generate commands. Run with -bootstrap.");
    return false;
}

void do_cmdlist(CHAR_DATA *ch, char *argument)
{
    BUFFER *buffer = new_buf();
    char buf[MSL];
    char cmd_colour[3];
    char line_colour[3];
    char helpstatus[15];

        CMD_DATA *command;
        int count = 0;

        add_buf(buffer, "Commands:\n");
        add_buf(buffer, "####  Name                Rank  Position    Log    Enabled  Function       Help  \n");
        add_buf(buffer, "----  ----               -----  --------  ------   -------  --------     --------\n");

        ITERATOR it;
        iterator_start(&it, commands_list);
        while((command = (CMD_DATA *)iterator_nextdata(&it)))
        {

            switch(command->type)
            {
                case CMDTYPE_NONE:
                    sprintf(cmd_colour, "{X");
                    break;
                case CMDTYPE_MOVE:
                    sprintf(cmd_colour, "{Y");
                    break;
                case CMDTYPE_COMBAT:
                    sprintf(cmd_colour, "{R");
                    break;
                case CMDTYPE_OBJECT:
                    sprintf(cmd_colour, "{J");
                    break;
                case CMDTYPE_INFO:
                    sprintf(cmd_colour, "{C");
                    break;
                case CMDTYPE_COMM:
                    sprintf(cmd_colour, "{M");
                    break;
                case CMDTYPE_RACIAL:
                    sprintf(cmd_colour, "{B");
                    break;
                case CMDTYPE_OOC:
                    sprintf(cmd_colour, "{G");
                    break;
                case CMDTYPE_IMMORTAL:
                    sprintf(cmd_colour, "{A");
                    break;
                case CMDTYPE_OLC:
                    sprintf(cmd_colour, "{P");
                    break;
                case CMDTYPE_ADMIN:
                    sprintf(cmd_colour, "{O");
                    break;
                default:
                    sprintf(cmd_colour, "{X");
                    break;
            }

            if (!command->enabled)
            {
                cmd_colour[1] = LOWER(cmd_colour[1]);
                sprintf(line_colour, "{D");
            }
            else
            {
                sprintf(line_colour, "{X");
            }

            if ((command->help_keywords == NULL || lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) == NULL) && command->summary == NULL) 
                sprintf(helpstatus, "{RNone{X");
            else if ((command->help_keywords == NULL || lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) == NULL) && command->summary != NULL)
                sprintf(helpstatus, "{YSummary{X");
            else if ((command->help_keywords != NULL && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL) && command->summary == NULL)
                sprintf(helpstatus, "{YKeywords{X");
            else
                sprintf(helpstatus, "{GBoth{X");

            sprintf(buf, "{W%3d{X)  \t<send href=\"cmdshow %s|cmdedit %s\" hint=\"Show %s|Edit %s\">%s%s%s\t</send>%s%s %-5.5s  %8s  %6s  %8s  %-12.12s %-12s{X\n\r",
                list_getindex(commands_list, command),
                command->name,
                command->name,
                command->name,
                command->name,
                cmd_colour,
                command->name,
                line_colour,
                pad_string(command->name, 20, NULL, NULL),
                line_colour,
                flag_string(staff_ranks, command->rank),
                position_table[command->position].name,
                log_flags[command->log].name,
                command->enabled ? "Enabled" : "Disabled",
                command->function ? do_func_name(command->function) : "None",
                helpstatus);
            add_buf(buffer, buf);
            count++;
        }
        iterator_stop(&it);

        sprintf(buf, "\n%d commands found.\n", count);
        add_buf(buffer, buf);
    

    if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
    {
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    }
    else
    {
        page_to_char(buffer->string, ch);
    }

    free_buf(buffer);
}

CMDEDIT( cmdedit_create )
{
    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  create <name>\n\r", ch);
        return false;
    }

    smash_tilde(argument);
    if (get_cmd_data(argument) != NULL)
    {
        send_to_char("That name is already in use.\n\r", ch);
        return false;
    }

    CMD_DATA *command = new_cmd();
    command->name = str_dup(argument);
    command->type = CMDTYPE_NONE;
    command->addl_types = CMD_TYPE_NONE;
    insert_command(command);

    ch->desc->pEdit = (void *)command;

    send_to_char("Command created.\n\r", ch);
    return true;
}

/**
 * cmdedit_show - Display current command data
 *
 * Uses the unified OLC display framework for consistent formatting.
 *
 * @param ch        Character viewing
 * @param argument  Unused
 * @return          false (no data changed)
 */
CMDEDIT (cmdedit_show)
{
    CMD_DATA *command;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&cmdedit_def);
    OLC_LAYOUT_CTX *ctx;

    EDIT_CMD(ch, command);
    cmdedit_normalize_command(command);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "CMDEdit", command->name,
        formatf("#%d", list_getindex(commands_list, command)), &cmdedit_def);

    olc_display_string(ctx, theme, "Name:", "name", command->name);
    olc_display_string(ctx, theme, "Type:", "type",
        command_types[command->type].name);
    olc_display_flags(ctx, theme, "Add'l Types:", "additional",
        command_addl_types, command->addl_types);
    olc_display_string(ctx, theme, "Rank:", "rank",
        flag_string(staff_ranks, command->rank));
    olc_display_string(ctx, theme, "Position:", "position",
        position_table[command->position].name);
    olc_display_string(ctx, theme, "Log:", "log",
        log_flags[command->log].name);
    olc_display_number(ctx, theme, "Order:", "order",
        list_getindex(commands_list, command));
    olc_display_bool(ctx, theme, "Enabled:", "enabled", command->enabled);

    if (!command->enabled || !IS_NULLSTR(command->reason))
        olc_display_string(ctx, theme, "{rReason:{x", "reason",
            !IS_NULLSTR(command->reason) ? command->reason : "(none)");

    olc_display_string(ctx, theme, "Function:", "function",
        command->function ? do_func_name(command->function) : "None");

    /* Help keywords with link */
    if (command->help_keywords != NULL
        && lookup_help_exact(command->help_keywords->string,
            get_staff_rank(ch), topHelpCat) != NULL) {
        HELP_DATA *pHelp = lookup_help_exact(
            command->help_keywords->string, get_staff_rank(ch), topHelpCat);
        olc_display_string(ctx, theme, "Help:", "sethelp",
            formatf("'\t<send href=\"help #%d\">{W%s{x\t</send>' ({W#%d{x)",
                pHelp->index, command->help_keywords->string, pHelp->index));
    } else if (command->help_keywords != NULL) {
        olc_display_string(ctx, theme, "Help:", "sethelp",
            formatf("{R%s{x", command->help_keywords->string));
    } else {
        olc_display_string(ctx, theme, "Help:", "sethelp", "(none set)");
    }

    olc_display_string(ctx, theme, "Summary:", "summary",
        command->summary);
    olc_display_flags(ctx, theme, "Flags:", "flags",
        command_flags, command->command_flags);

    olc_display_text(ctx, theme, "Description:", "description",
        command->description);

    olc_display_section(ctx, theme, "Coders' Comments");
    olc_display_text(ctx, theme, NULL, "comments", command->comments);

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

CMDEDIT( cmdedit_delete )
{
    CMD_DATA *command;
    EDIT_CMD(ch, command);

    if (str_cmp(argument, "confirm")) {
        send_to_char("Type 'delete confirm' to permanently delete this command.\n\r", ch);
        send_to_char("You can also delete from outside the editor with:\n\r", ch);
        printf_to_char(ch, "  cmdedit %s delete %s\n\r", command->name, command->name);
        return false;
    }

    if (!cmdedit_delete_command(ch, command))
        return false;

    edit_done(ch);
    return true;
}

CMDEDIT( cmdedit_name )
{
    CMD_DATA *command;

    EDIT_CMD(ch, command);

    smash_tilde(argument);
    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  name <name>\n\r", ch);
        return false;
    }

    CMD_DATA *other = get_cmd_data(argument);
    if (other && other != command)
    {
        send_to_char("That name is already in use.\n\r", ch);
        return false;
    }

    free_string(command->name);
    command->name = str_dup(argument);
    list_remlink(commands_list, command, false);
    insert_command(command);

    send_to_char("COMMAND Name set.\n\r", ch);
    return true;
}

/**
 * cmdedit_description - Edit the command description (opens string editor)
 */
CMDEDIT( cmdedit_description )
{
    CMD_DATA *command;
    EDIT_CMD(ch, command);
    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &command->description, NULL, NULL);
}

/**
 * cmdedit_comments - Edit the coders' comments (opens string editor)
 */
CMDEDIT( cmdedit_comments )
{
    CMD_DATA *command;
    EDIT_CMD(ch, command);
    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &command->comments, NULL, NULL);
}

CMDEDIT( cmdedit_type )
{
    CMD_DATA *command;
    EDIT_CMD( ch, command );
    char buf[MAX_STRING_LENGTH];

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  type <type>\n\r", ch);
        send_to_char("Please select one of the following:\n\r", ch);
        for(int i = 0; command_types[i].name; i++)
        {
            send_to_char(formatf(" %s\n\r", command_types[i].name), ch);
        }
        return false;
    }

    long type;
    if ((type = flag_value(command_types, argument)) == NO_FLAG)
    {
        send_to_char("Invalid type.\n\r", ch);
        return false;
    }



    command->type = type;

    cmdedit_normalize_command(command);

    sprintf(buf, "Type set to %s.\n\r", command_types[type].name);
    send_to_char(buf,ch);
    return true;
}

CMDEDIT (cmdedit_rank )
{

    CMD_DATA *command;
    EDIT_CMD( ch, command );

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  rank <rank>\n\r", ch);
        send_to_char("Please select one of the following:\n\r", ch);
        for(int i = 0; staff_ranks[i].name; i++)
        {
            if (staff_ranks[i].settable && staff_ranks[i].bit <= get_staff_rank(ch))
            {
                send_to_char(formatf(" %s\n\r", staff_ranks[i].name), ch);
            }
        }
        return false;
    }

    int new_rank;
    if ((new_rank = flag_value(staff_ranks, argument)) == NO_FLAG)
    {
        send_to_char("Invalid rank.\n\r", ch);
        return false;
    }

    if (new_rank > get_staff_rank(ch))
    {
        send_to_char("You cannot set a command to a rank higher than your own.\n\r", ch);
        return true;
    }

    command->rank = new_rank;
    send_to_char("Minimum rank set.\n\r", ch);
    return true;

}

/**
 * cmdedit_position - Set the minimum position to use this command
 */
CMDEDIT (cmdedit_position )
{
    CMD_DATA *command;
    EDIT_CMD(ch, command);
    return olc_cmd_type_set_i16(ch, argument, "Position",
        "Syntax: position <position>\n\rType '? position' for a list of positions.",
        &command->position, position_flags, NULL, NULL);
}

/**
 * cmdedit_log - Set the command log level
 */
CMDEDIT (cmdedit_log )
{
    CMD_DATA *command;
    EDIT_CMD(ch, command);
    return olc_cmd_type_set_i16(ch, argument, "Log", NULL,
        &command->log, log_flags, NULL, NULL);
}

CMDEDIT( cmdedit_enabled )
{
    CMD_DATA *command;

    EDIT_CMD(ch, command);

    if (!str_cmp(argument,"yes")) {
        command->enabled = true;
        send_to_char("Command has been enabled.\n\r", ch);
    } else if (!str_cmp(argument,"no")) {
        if (!command->function)
        {
            send_to_char("Command must have a function assigned before it can be enabled.\n\r", ch);
            return false;
        }
        command->enabled = false;
        send_to_char("Command has been disabled.\n\r", ch);
    } else {
        send_to_char("Syntax:  enabled yes|no\n\r", ch);
        return false;
    }

    return true;
}

CMDEDIT ( cmdedit_reason )
{
    CMD_DATA *command;
    char arg[MAX_INPUT_LENGTH];

    EDIT_CMD(ch, command);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  reason <set <string>|clear>\n\r", ch);
    return false;
    }

    argument = one_argument(argument, arg);

    if (!str_cmp(arg, "clear"))
    {
        free_string(command->reason);
        command->reason = str_dup("");
        send_to_char("Command disabled reason cleared.\n\r", ch);
        return true;
    }
    else if (!str_cmp(arg, "set"))
    {
        if (argument[0] == '\0')
        {
            send_to_char("Syntax:  reason set <string>\n\r", ch);
            return false;
        }
            
        free_string(command->reason);
        if (str_suffix("{x", argument))
            strcat(argument, "{x");
        command->reason = str_dup(argument);
        command->reason[0] = UPPER(command->reason[0] );

        send_to_char("Command disabled reason set.\n\r", ch);
        return true;

    }
    else
    {
        send_to_char("Syntax:  reason <set <string>|clear>\n\r", ch);
        return false;
    }
    return false;
}

/**
 * cmdedit_flags - Toggle command flags
 */
CMDEDIT( cmdedit_flags )
{
    CMD_DATA *command;
    EDIT_CMD(ch, command);
    return olc_cmd_flag_toggle(ch, argument, "Flags",
        "Syntax: flags <flag>\n\rType '? cmd' for valid list.",
        &command->command_flags, command_flags, NULL, NULL);
}

CMDEDIT (cmdedit_function )
{
    char arg[MIL];
    CMD_DATA *command;
    char buf[MAX_STRING_LENGTH];

    EDIT_CMD(ch, command);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: function <set <name>|clear>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if (!str_prefix(arg, "set"))
    {
        if (argument[0] == '\0')
        {
            send_to_char("Syntax: function set <name>\n\r", ch);
            send_to_char("Invalid do_ function. Use '\t<send href=\"? do_func\">? do_func\t</send>' for a list of functions.\n\r", ch);
            return false;
        }
    
    
        DO_FUN *func = do_func_lookup(argument);
        if (func == NULL)
        {
            send_to_char("Syntax: function set <name>\n\r", ch);
            send_to_char("Invalid do_ function. Use '\t<send href=\"? do_func\">? do_func\t</send>' for a list of functions.", ch);
            return false;
        }

        command->function = func;
        sprintf(buf, "Function set to %s.\n\r", argument);
        send_to_char(buf, ch);
        return true;
    }
    
    else if (!str_prefix(arg, "clear"))
    {
        command->function = NULL;
        command->enabled = false;
        send_to_char("Command function cleared. Command disabled.\n\r", ch);
        return true;
    }

    else
    {
        send_to_char("Syntax: function <set <name>|clear>\n\r", ch);
        return false;
    }

    //send_to_char("WIP\n\r", ch);
    return false;
}

CMDEDIT (cmdedit_help )
{

    CMD_DATA *command;
    EDIT_CMD( ch, command );
    STRING_DATA *help;
    char buf[MAX_STRING_LENGTH];
    HELP_DATA *pHelp;

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: sethelp [keywords]\n\r",ch);
        return false;
    }

    if (!str_cmp(argument, "clear"))
    {
        free_string_data(command->help_keywords);
        command->help_keywords = NULL;
        send_to_char("Help keywords cleared.\n\r", ch);
        return true;
    }

    if (argument[0] == '#')
    {
        argument++;
        int index;
        if ((index = atoi(argument)) < 0 || index > 32000)
        {
            send_to_char("That help index is out of range.\n\r", ch);
            return false;
        } else 
            pHelp = lookup_help_index(index, get_staff_rank(ch), topHelpCat);
        
        if (pHelp == NULL)
        {
            act("There is no helpfile with index $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
            return false;            
        }
        
    }
    else
    {
        pHelp = lookup_help_exact(argument, get_staff_rank(ch), topHelpCat);
        if (pHelp == NULL)
        {
            act("There is no helpfile with keywords $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
            return false;
        }
    }

    int i = 0;
    while (argument[i] != '\0')
    {
    argument[i] = UPPER(argument[i]);
    i++;
    }

    help = new_string_data();
    help->string = str_dup(pHelp->keyword);
    command->help_keywords = help;
    sprintf(buf, "Help keywords set to %s.\n\r", pHelp->keyword);
    send_to_char(buf, ch);
    return true;
}

/**
 * cmdedit_summary - Set the command summary line
 */
CMDEDIT ( cmdedit_summary )
{
    CMD_DATA *command;
    EDIT_CMD(ch, command);
    return olc_cmd_string(ch, argument, "Summary", NULL, &command->summary,
        OLC_STR_DEFAULT, NULL, NULL);
}

CMDEDIT ( cmdedit_order )
{

/*
    send_to_char("Disabled pending further work.\n\r", ch);
    return false;
*/

    CMD_DATA *command;

    EDIT_CMD(ch, command);
    int curorder = list_getindex(commands_list,command);
    char buf[MSL], arg2[MIL], arg3[MIL];
    //sprintf(buf, "Current order: %d, desired order %d\n\r", curorder, atoi(argument));
    //send_to_char(buf, ch);

    
    argument = one_argument(argument, arg2);

    if (!is_number(arg2))
    {
        send_to_char("Invalid number.\n\r", ch);
        return false;
    }

    int index = atoi(arg2);
    if (index < 0 || index > list_size(commands_list))
    {
        sprintf(buf, "Invalid number.  Must be between 0 and %d.\n\r", list_size(commands_list));
        send_to_char(buf, ch);
        return false;
    }

    if (index != curorder)
    {
        send_to_char("Invalid current position for command.\n\r", ch);
        return false;
    }

    int to_index = -1;
    argument = one_argument(argument, arg3);
    if (is_number(arg3))
    {
        to_index = atoi(arg3);
        if (to_index < 0 || to_index > list_size(commands_list))
        {
            sprintf(buf, "Invalid number.  Must be between 0 and %d.\n\r", list_size(commands_list));
            send_to_char(buf, ch);
            return false;
        }
    }
    else if (!str_prefix(arg3, "up"))
    {
        if (index <= 1)
        {
            sprintf(buf, "%s is already at the top of the list.\n\r", command->name);
            return false;
        }
        to_index = index - 1;
    }
    else if (!str_prefix(arg3, "down"))
    {
        if (index >= list_size(commands_list))
        {
            sprintf(buf, "%s is already at the bottom of the list.\n\r", command->name);
            return false;
        }
        to_index = index + 1;
    }
    else if (!str_prefix(arg3, "top") || !str_prefix(arg3, "first"))
    {
        if (index <= 1)
        {
            sprintf(buf, "%s is already at the top of the list.\n\r", command->name);
            return false;
        }
        to_index = 1;
    }
    else if (!str_prefix(arg3, "bottom") || !str_prefix(arg3, "last"))
    {
        if (index >= list_size(commands_list))
        {
            sprintf(buf, "%s is already at the bottom of the list.\n\r", command->name);
            return false;
        }
        to_index = list_size(commands_list);
    }
    else
    {
        send_to_char("Syntax: order <index> <up|down|top|first|bottom|last|index>\n\r", ch);
        return false;
    }

    if (index == to_index)
    {
        sprintf(buf, "%s is already at the desired position.\n\r", command->name);
        return false;
    }

    list_movelink(commands_list, index, to_index);
    sprintf(buf, "Attempted to move %s from position %d to position %d. Actually moved to %d\n\r", command->name, index, to_index, list_getindex(commands_list, command));
    send_to_char(buf, ch);
    return true;
}

/**
 * cmdedit_additional - Toggle additional command type flags
 */
CMDEDIT( cmdedit_additional )
{
    CMD_DATA *command;
    bool changed;

    EDIT_CMD(ch, command);
    cmdedit_normalize_command(command);

    changed = olc_cmd_flag_toggle(ch, argument, "Add'l Types",
        "Syntax: additional <type>\n\rType '? cmd_types' for valid list.",
        &command->addl_types, command_addl_types, NULL, NULL);

    if (!changed)
        return false;

    cmdedit_normalize_command(command);
    return true;
}

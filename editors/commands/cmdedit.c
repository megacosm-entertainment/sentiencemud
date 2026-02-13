/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*       ROM 2.4 is copyright 1993-1998 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@hypercube.org)                            *
*           Gabrielle Taylor (gtaylor@hypercube.org)                       *
*           Brian Moore (zump@rom.org)                                     *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

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

void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table);


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
        return true;

    log_string("commands.json not found, generating from cmd_table[]...");

    if (create_commands_json() && json_load_commands(COMMANDS_JSON_FILE))
    {
        log_string("Generated and loaded commands.json from cmd_table[]");
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
    insert_command(command);

    ch->pcdata->immortal->last_olc_command = current_time;
    olc_set_editor(ch, ED_CMDEDIT, command);

    send_to_char("Command created.\n\r", ch);
    return true;
}

CMDEDIT (cmdedit_show)
{
    CMD_DATA *command;

    EDIT_CMD(ch, command);

    BUFFER *buffer = new_buf();

    add_buf(buffer, formatf("Name:          %s\n\r", command->name));
    add_buf(buffer, formatf("Type:          {+%s\n\r", command_types[command->type].name));
    add_buf(buffer, formatf("Add'l Types    %s\n\r", flag_string(command_addl_types, command->addl_types)));
    add_buf(buffer, formatf("Rank:          {+%s\n\r", flag_string(staff_ranks, command->rank)));
    add_buf(buffer, formatf("Position:      {+%s\n\r", position_table[command->position].name));
    add_buf(buffer, formatf("Log:           {+%s\n\r", log_flags[command->log].name));
    add_buf(buffer, formatf("Order:         %d\n\r", list_getindex(commands_list, command)));
    add_buf(buffer, formatf("Enabled:       %s\n\r", command->enabled ? "Yes" : "No"));
    if (!command->enabled || !IS_NULLSTR(command->reason)) 
        add_buf(buffer, formatf("{rDisabled Reason{X: %s\n\r", !IS_NULLSTR(command->reason) ? command->reason : "(none)"));

    add_buf(buffer, formatf("Function:      %s\n\r", command->function ? do_func_name(command->function) : "None"));
    if (command->help_keywords != NULL && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL)
        add_buf(buffer, formatf("Help Keywords: '\t<send href=\"help #%d\">{W%s{X\t</send>' ({W#%d{X)\n\r", lookup_help_exact(command->help_keywords->string, get_staff_rank(ch), topHelpCat)->index, command->help_keywords->string, lookup_help_exact(command->help_keywords->string, get_staff_rank(ch), topHelpCat)->index));
    else if (command->help_keywords != NULL && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) == NULL)
        add_buf(buffer, formatf("Help Keywords: {R%s{X\n\r", command->help_keywords->string));
    else
        add_buf(buffer, formatf("Help Keywords: %s\n\r", "(none set)"));
    
    add_buf(buffer, formatf("Summary:       %s\n\r", command->summary ? command->summary : "(none)"));
    add_buf(buffer, formatf("Command Flags: %s\n\r", flag_string(command_flags, command->command_flags)));

    add_buf(buffer, formatf("\n\rDescription:\n\r   %s\n\r", string_indent(command->description,3)));

    add_buf(buffer, "\n\r-----\n\r{WCoders' Comments:{X\n\r");
    add_buf(buffer, command->comments);
    add_buf(buffer, "\n\r-----\n\r");

    if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
    {
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    }
    else
    {
        page_to_char(buffer->string, ch);
    }

    free_buf(buffer);
    return false;
}

CMDEDIT( cmdedit_delete )
{
    send_to_char("WIP\n\r",ch);
    return false;
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

CMDEDIT( cmdedit_description )
{
    CMD_DATA *command;

    EDIT_CMD(ch, command);

    if (argument[0] == '\0')
    {
        string_append(ch, &command->description);
        return true;
    }

    send_to_char("Syntax:  description\n\r", ch);
    return false;
}

CMDEDIT( cmdedit_comments )
{
    CMD_DATA *command;

    EDIT_CMD(ch, command);

    if (argument[0] == '\0')
    {
        string_append(ch, &command->comments);
        return true;
    }

    send_to_char("Syntax:  comments\n\r", ch);
    return false;
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

    if (!IS_SET(command->addl_types, flag_value(command_addl_types, flag_name(command_types, command->type))))
        TOGGLE_BIT(command->addl_types, flag_value(command_addl_types, flag_name(command_types, command->type)));

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

CMDEDIT (cmdedit_position )
{
    CMD_DATA *command;
    char arg[MAX_INPUT_LENGTH];
    int value;

    EDIT_CMD(ch, command);
    argument = one_argument(argument, arg);
    if (arg[0] == '\0')
    {
        send_to_char("Syntax:  position [position]\n\r", ch);
        send_to_char("Type '\t<send href=\"? position\">? position\t</send>' for a list of positions.\n\r", ch);
        return false;
    }

    if (argument[0] == '\0')
    {
        if ((value = flag_value(position_flags, arg)) == NO_FLAG)
            return false;

        command->position = value;
        send_to_char("Minimum command position set.\n\r", ch);
        return true;
    }
    return false;
}

CMDEDIT (cmdedit_log )
{

    CMD_DATA *command;
    EDIT_CMD( ch, command );

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  log <level>\n\r", ch);
        send_to_char("Please select one of the following:\n\r", ch);
        for(int i = 0; log_flags[i].name; i++)
        {
            send_to_char(formatf(" %s\n\r", log_flags[i].name), ch);
        }
        return false;
    }

    int log;
    if ((log = flag_value(log_flags, argument)) == NO_FLAG)
    {
        send_to_char("Invalid log level.\n\r", ch);
        return false;
    }

    command->log = log;
    send_to_char("Log level set.\n\r", ch);
    return true;

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

CMDEDIT( cmdedit_flags )
{
    CMD_DATA *command;

    EDIT_CMD(ch, command);

    long value;
    if ((value = flag_value(command_flags, argument)) == NO_FLAG)
    {
        send_to_char("Invalid command flag.  Use '\t<send href=\"? cmd\">? cmd\t</send>' for valid list.\n\r", ch);
        show_flag_cmds(ch, command_flags);
        return false;
    }

    TOGGLE_BIT(command->command_flags, value);

    send_to_char("Command Flags toggled.\n\r", ch);
    return true;
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

CMDEDIT ( cmdedit_summary )
{
    CMD_DATA *command;

    EDIT_CMD(ch, command);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  summary [string]\n\r", ch);
    return false;
    }

    free_string(command->summary);

    command->summary = str_dup(argument);
    command->summary[0] = UPPER(command->summary[0] );

    send_to_char("Command summary set.\n\r", ch);
    return true;
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

CMDEDIT( cmdedit_additional )
{
    CMD_DATA *command;

    EDIT_CMD(ch, command);

    long value;
    if ((value = flag_value(command_addl_types, argument)) == NO_FLAG)
    {
        send_to_char("Invalid command flag.  Use '\t<send href=\"? cmd_types\">? cmd_types\t</send>' for valid list.\n\r", ch);
        show_flag_cmds(ch, command_types);
        return false;
    }

    TOGGLE_BIT(command->addl_types, value);

    send_to_char("Additional command types toggled.\n\r", ch);
    return true;
}

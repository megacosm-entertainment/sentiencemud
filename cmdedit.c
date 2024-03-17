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
#include "strings.h"
#include "merc.h"
#include "interp.h"
#include "db.h"
#include "math.h"
#include "recycle.h"
#include "tables.h"
#include "olc_save.h"
#include "scripts.h"
#include "wilds.h"


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

FUNC_LOOKUPS(do, DO_FUN,(!func || func == cmd_under_construction))

// Create a function to load commands from a command file. If the command file does not exist, boostrap one from the cmd_table.

void save_command(FILE *fp, CMD_DATA *command)
{
    fprintf(fp, "#COMMAND %s~\n", command->name);
    fprintf(fp, "Enabled %d\n", command->enabled);
    fprintf(fp, "Function %s~\n", do_func_name(command->function));
    fprintf(fp, "Level %d\n", command->level);
    fprintf(fp, "Log %d\n", command->log);
    fprintf(fp, "Position %d\n", command->position);
    fprintf(fp, "Show %d\n", command->show);
    fprintf(fp, "Comments %s~\n", command->comments);
    fprintf(fp, "Description %s~\n", command->description);
    fprintf(fp, "HelpKeywords %s~\n", command->help_keywords);
    fprintf(fp, "#-COMMAND\n");
}

void save_commands()
{
    FILE *fp;

    log_string("save_commands: saving " COMMANDS_FILE);
    if ((fp = fopen(COMMANDS_FILE, "w")) == NULL)
    {
        bug("save_commands: fopen", 0);
        perror(COMMANDS_FILE);
    }
    else
    {
        log_string(formatf("save_commands: Saving %ld commands", commands_list->size));
        
        ITERATOR it;
        CMD_DATA *command;

        int count = 0;
        iterator_start(&it, commands_list);
        while((command = (CMD_DATA *)iterator_nextdata(&it)))
        {
            count++;
        }
        iterator_stop(&it);
        log_string(formatf("Found %d commands from iterating. (save_commands)", count));

        iterator_start(&it, commands_list);
        while((command = (CMD_DATA *)iterator_nextdata(&it)))
        {
            log_string(formatf("Saving command '%s'", command->name));
            save_command(fp, command);
        }
        iterator_stop(&it);
        fprintf(fp, "#END\n");
        fclose(fp);
    }
}

void insert_command(CMD_DATA *command)
{
    ITERATOR it;
    CMD_DATA *cmd;
    iterator_start(&it, commands_list);
    while((cmd = (CMD_DATA *)iterator_nextdata(&it)))
    {
        int cmp = str_cmp(command->name, cmd->name);
        if (cmp < 0)
        {
            iterator_insert_before(&it, command);
            log_string(formatf("DBG2 Inserted command '%s', commands_list is now %ld entries long", command->name, commands_list->size));
            break;
        }
    }
    iterator_stop(&it);

    if (!cmd)
    {
        list_appendlink(commands_list, command);
        log_string(formatf("DBG1 Inserted command '%s', commands_list is now %ld entries long", command->name, commands_list->size));
    }

    

}

CMD_DATA *load_command(FILE *fp)
{
    CMD_DATA *command;
    char *word;
    bool fMatch;


    command = new_cmd();
    command->name = fread_string(fp);

    while(str_cmp((word = fread_word(fp)), "#-COMMAND"))
    {
        fMatch = true;

        switch(word[0])
        {
            case 'C':
                KEY("Comments", command->comments, fread_string(fp));
                break;
            case 'D':
                KEY("Description", command->description, fread_string(fp));
                break;
            case 'E':
                KEY("Enabled", command->enabled, fread_number(fp));
                break;
            case 'H':
                KEY("HelpKeywords", command->help_keywords, fread_string(fp));
                break;
            case 'F':
            if (!str_cmp(word, "Function"))
            {
                char *name = fread_string(fp);
                command->function = do_func_lookup(name);
                fMatch = true;
                break;
            }
                break;
            case 'L':
                KEY("Level", command->level, fread_number(fp));
                KEY("Log", command->log, fread_number(fp));
                break;
            case 'P':
                KEY("Position", command->position, fread_number(fp));
                break;
            case 'S':
                KEY("Show", command->show, fread_number(fp));
                break;
        }

        if (!fMatch)
        {
            bug(formatf("load_command: no match for '%s'\n\r", word), 0);
            fread_to_eol(fp);
        }
    }
    return command;
}

static void delete_command(void *ptr)
{
    free_cmd((CMD_DATA *)ptr);
}

bool load_commands()
{
    FILE *fp;
    CMD_DATA *command;

    commands_list = list_createx(false, NULL, delete_command);

    if (!IS_VALID(commands_list))
    {
        log_string("load_commands: commands_list is not valid.");
        return false;
    }

    log_string("load_commands: loading " COMMANDS_FILE);
    if ((fp = fopen(COMMANDS_FILE, "r")) == NULL)
    {
        log_string("load_commands: " COMMANDS_FILE " not found. Bootstrapping from cmd_table.");

        for (int i = 0; cmd_table[i].name; i++)
        {
            if (*cmd_table[i].name == '\0')
                continue;
            
            log_string(formatf("Bootstrapping command '%s'", cmd_table[i].name));
            command = new_cmd();
            command->name = str_dup(cmd_table[i].name);
            command->level = cmd_table[i].level;
            command->log = cmd_table[i].log;
            command->position = cmd_table[i].position;
            command->show = cmd_table[i].show;
            command->function = cmd_table[i].do_fun;

            command->enabled = true;

            insert_command(command);
            log_string(formatf("DBG3 Bootstrapped command '%s', commands_list is now %ld entries long", command->name, commands_list->size));
        }
        save_commands();
    }
    else
    {
        char *word;
        bool fMatch;

        while(str_cmp((word = fread_word(fp)), "#END"))
        {
            fMatch = true;

            switch(word[0])
            {
                case '#':
                    if (!str_cmp(word, "#COMMAND"))
                    {
                        command = load_command(fp);
                        
                        insert_command(command);
                        fMatch = true;
                        break;
                    }
                    break;
            }

            if (!fMatch)
            {
                bug(formatf("load_commands: no match for word '%s'", word),0);
                fread_to_eol(fp);
            }
        }
    }
    log_string(formatf("load_commands: Loaded %ld commands", commands_list->size));
    ITERATOR it;
            int count = 0;
        iterator_start(&it, commands_list);
        while((command = (CMD_DATA *)iterator_nextdata(&it)))
        {
            count++;
        }
        iterator_stop(&it);
        log_string(formatf("Found %d commands from iterating. (load_commands)", count));

    return true;
}

void do_cmdlist(CHAR_DATA *ch, char *argument)
{
    BUFFER *buffer = new_buf();
    char buf[MSL];

    if (IS_NULLSTR(argument))
    {
        CMD_DATA *command;
        int count = 0;

        add_buf(buffer, "Commands:\n");
        add_buf(buffer, "Name                 Level  Position  Log  Show  Enabled  Function\n");
        add_buf(buffer, "----                 -----  --------  ---  ----  -------  --------\n");

        ITERATOR it;
        iterator_start(&it, commands_list);
        while((command = (CMD_DATA *)iterator_nextdata(&it)))
        {
            sprintf(buf, "%-20s %5d  %8s  %3d  %4d  %7s  %s\n",
                command->name,
                command->level,
                position_table[command->position].name,
                command->log,
                command->show,
                command->enabled ? "Yes" : "No",
                command->function ? do_func_name(command->function) : "None");
            add_buf(buffer, buf);
            count++;
        }
        iterator_stop(&it);

        sprintf(buf, "\n%d commands found.\n", count);
        add_buf(buffer, buf);
    }
    else
    {
        CMD_DATA *command;
        bool found = false;

        ITERATOR it;
        iterator_start(&it, commands_list);
        while((command = (CMD_DATA *)iterator_nextdata(&it)))
        {
            if (!str_prefix(argument, command->name))
            {
                found = true;
                sprintf(buf, "Name: %s\n", command->name);
                add_buf(buffer, buf);
                sprintf(buf, "Level: %d\n", command->level);
                add_buf(buffer, buf);
                sprintf(buf, "Position: %s\n", position_table[command->position].name);
                add_buf(buffer, buf);
                sprintf(buf, "Log: %d\n", command->log);
                add_buf(buffer, buf);
                sprintf(buf, "Show: %d\n", command->show);
                add_buf(buffer, buf);
                sprintf(buf, "Enabled: %s\n", command->enabled ? "Yes" : "No");
                add_buf(buffer, buf);
                sprintf(buf, "Function: %s\n", command->function ? do_func_name(command->function) : "None");
                add_buf(buffer, buf);
                sprintf(buf, "Comments: %s\n", command->comments);
                add_buf(buffer, buf);
                sprintf(buf, "Description: %s\n", command->description);
                add_buf(buffer, buf);
                sprintf(buf, "Help Keywords: %s\n", command->help_keywords);
                add_buf(buffer, buf);
            }
        }
    }

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
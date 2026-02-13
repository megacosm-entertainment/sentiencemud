/**
 * bootstrap_commands.c - Command table generation for bootstrap
 *
 * Generates commands.json from the hardcoded cmd_table[] array,
 * using the function lookup table from editors/commands/cmdedit.c
 */

#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include "../merc.h"
#include "../interp.h"
#include "bootstrap_internal.h"

/* External declarations for command table and function lookup */
extern const struct cmd_type cmd_table[];
extern char *do_func_name(DO_FUN *func);

/**
 * create_commands_json - Generate commands.json from cmd_table
 *
 * Reads the hardcoded cmd_table[] array and serializes it to JSON format.
 * Uses do_func_name() from cmdedit.c to lookup function names from do_func_table[].
 * This allows the game to bootstrap without relying on an existing commands file.
 *
 * @return true on success
 */
bool create_commands_json(void)
{
    json_t *root = json_object();
    json_t *commands_array = json_array();
    int command_count = 0;

    json_object_set_new(root, "version", json_integer(1));

    /* Iterate through cmd_table and convert each entry */
    for (int i = 0; cmd_table[i].name != NULL; i++) {
        json_t *cmd = json_object();

        json_object_set_new(cmd, "name", json_string(cmd_table[i].name));
        json_object_set_new(cmd, "enabled", json_boolean(true));

        /* Get function name from do_func_table via do_func_name() */
        char *func_name = do_func_name(cmd_table[i].do_fun);
        json_object_set_new(cmd, "function", json_string(func_name ? func_name : ""));

        /* NOTE: cmd_table[].level values use the old level macros (ML/L1-L6/IM/HE)
         * which are NOT valid staff_rank values. The bootstrap mapping is only
         * meaningful if cmd_table[] is updated to use STAFF_* constants, or if
         * a conversion function is added. The existing commands.json already
         * has correct rank values from prior OLC editing. */
        json_object_set_new(cmd, "rank", json_integer(cmd_table[i].level));
        json_object_set_new(cmd, "log", json_integer(cmd_table[i].log));
        json_object_set_new(cmd, "position", json_integer(cmd_table[i].position));

        /* Determine command type based on characteristics */
        int cmd_type = CMDTYPE_NONE;
        if (cmd_table[i].is_ooc) {
            cmd_type = CMDTYPE_OOC;
        } else if (strstr(cmd_table[i].name, "north") || strstr(cmd_table[i].name, "south") ||
                   strstr(cmd_table[i].name, "east") || strstr(cmd_table[i].name, "west") ||
                   strstr(cmd_table[i].name, "up") || strstr(cmd_table[i].name, "down") ||
                   !strcmp(cmd_table[i].name, "ne") || !strcmp(cmd_table[i].name, "nw") ||
                   !strcmp(cmd_table[i].name, "se") || !strcmp(cmd_table[i].name, "sw")) {
            cmd_type = CMDTYPE_MOVE;
        }
        json_object_set_new(cmd, "type", json_integer(cmd_type));

        json_object_set_new(cmd, "addl_types", json_integer(0));

        /* Command flags: show flag from cmd_table */
        json_object_set_new(cmd, "command_flags", json_integer(cmd_table[i].show));

        /* Empty metadata fields (can be filled in via OLC later) */
        json_object_set_new(cmd, "comments", json_string(""));
        json_object_set_new(cmd, "description", json_string(""));
        json_object_set_new(cmd, "help_keywords", json_string(""));
        json_object_set_new(cmd, "reason", json_string(""));
        json_object_set_new(cmd, "summary", json_string(""));

        json_array_append_new(commands_array, cmd);
        command_count++;
    }

    json_object_set_new(root, "commands", commands_array);

    /* Write to file with sorted keys for consistency */
    if (json_dump_file(root, "data/system/commands.json", JSON_INDENT(2) | JSON_SORT_KEYS) != 0) {
        fprintf(stderr, "Failed to write commands.json\n");
        json_decref(root);
        return false;
    }

    json_decref(root);
    printf("  Generated %d commands from cmd_table[]\n", command_count);
    return true;
}

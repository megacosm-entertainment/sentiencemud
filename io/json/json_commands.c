/***************************************************************************
 *  JSON Command Serialization - Implementation                            *
 *                                                                         *
 *  Converts command data between C structures and JSON format.            *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../db.h"
#include "../../recycle.h"
#include "../../log.h"
#include "json_commands.h"
#include "json_common.h"

#define JSON_COMMANDS_VERSION 1

extern DO_FUN *do_func_lookup(char *name);
extern char *do_func_name(DO_FUN *func);
extern void insert_command(CMD_DATA *command);

/**
 * json_command_serialize - Serialize a single command to JSON
 *
 * @param cmd  Command data to serialize
 * @return     JSON object or NULL on error (caller must json_decref)
 */
static json_t *json_command_serialize(CMD_DATA *cmd)
{
    if (!cmd) return NULL;

    json_t *obj = json_object();

    json_object_set_new(obj, "name", json_string_safe(cmd->name));
    json_object_set_new(obj, "enabled", cmd->enabled ? json_true() : json_false());
    json_object_set_new(obj, "function", json_string(cmd->function ? do_func_name(cmd->function) : ""));
    json_object_set_new(obj, "rank", json_integer(cmd->rank));
    json_object_set_new(obj, "log", json_integer(cmd->log));
    json_object_set_new(obj, "position", json_integer(cmd->position));
    json_object_set_new(obj, "type", json_integer(cmd->type));
    json_object_set_new(obj, "addl_types", json_integer(cmd->addl_types));
    json_object_set_new(obj, "command_flags", json_integer(cmd->command_flags));
    json_object_set_new(obj, "comments", json_string_safe(cmd->comments));
    json_object_set_new(obj, "description", json_string_safe(cmd->description));

    if (cmd->help_keywords != NULL && !IS_NULLSTR(cmd->help_keywords->string))
        json_object_set_new(obj, "help_keywords", json_string(cmd->help_keywords->string));
    else
        json_object_set_new(obj, "help_keywords", json_string(""));

    json_object_set_new(obj, "reason", json_string_safe(cmd->reason));
    json_object_set_new(obj, "summary", json_string_safe(cmd->summary));

    return obj;
}

/**
 * json_command_deserialize - Deserialize a single command from JSON
 *
 * @param json  JSON object to deserialize
 * @return      Newly allocated CMD_DATA or NULL on error
 */
static CMD_DATA *json_command_deserialize(json_t *json)
{
    if (!json || !json_is_object(json)) return NULL;

    CMD_DATA *cmd = new_cmd();

    cmd->name = str_dup(json_get_string_default(json, "name", ""));
    cmd->enabled = json_get_bool_default(json, "enabled", true);
    cmd->rank = (int16_t)json_get_int_default(json, "rank", 0);
    cmd->log = (int16_t)json_get_int_default(json, "log", 0);
    cmd->position = (int16_t)json_get_int_default(json, "position", 0);
    cmd->type = json_get_int_default(json, "type", 0);
    cmd->addl_types = json_get_int_default(json, "addl_types", 0);
    cmd->command_flags = json_get_int_default(json, "command_flags", 0);
    cmd->comments = str_dup(json_get_string_default(json, "comments", ""));
    cmd->description = str_dup(json_get_string_default(json, "description", ""));
    cmd->reason = str_dup(json_get_string_default(json, "reason", ""));
    cmd->summary = str_dup(json_get_string_default(json, "summary", ""));

    const char *func_name = json_get_string_default(json, "function", "");
    if (!IS_NULLSTR(func_name)) {
        cmd->function = do_func_lookup((char *)func_name);
        if (!cmd->function) {
            perrf(LOG_ERROR, "json_command_deserialize: Unknown function '%s' for command '%s' - disabling",
                func_name, cmd->name);
            cmd->enabled = false;
        }
    }

    const char *help_kw = json_get_string_default(json, "help_keywords", "");
    if (!IS_NULLSTR(help_kw)) {
        STRING_DATA *help = new_string_data();
        help->string = str_dup(help_kw);
        cmd->help_keywords = help;
    }

    return cmd;
}

/**
 * json_save_commands - Save all commands to a JSON file
 *
 * @param path  File path to write
 * @return      true on success, false on failure
 */
bool json_save_commands(const char *path)
{
    json_t *root = json_object();
    json_object_set_new(root, "version", json_integer(JSON_COMMANDS_VERSION));

    json_t *arr = json_array();

    ITERATOR it;
    CMD_DATA *cmd;
    iterator_start(&it, commands_list);
    while ((cmd = (CMD_DATA *)iterator_nextdata(&it))) {
        json_t *obj = json_command_serialize(cmd);
        if (obj)
            json_array_append_new(arr, obj);
    }
    iterator_stop(&it);

    json_object_set_new(root, "commands", arr);

    if (!json_file_save(root, path, "json_save_commands", JSON_INDENT(2) | JSON_SORT_KEYS))
        return false;

    plogf(LOG_OLC, "json_save_commands: saved %ld commands to %s", commands_list->size, path);
    return true;
}

/**
 * json_load_commands - Load commands from a JSON file
 *
 * Expects commands_list to already be initialized before calling.
 *
 * @param path  File path to read
 * @return      true on success, false on failure
 */
bool json_load_commands(const char *path)
{
    json_t *arr;
    json_t *root = json_file_load(path, "commands", &arr, "json_load_commands");
    if (!root)
        return false;

    size_t index;
    json_t *value;
    json_array_foreach(arr, index, value) {
        CMD_DATA *cmd = json_command_deserialize(value);
        if (cmd)
            insert_command(cmd);
    }

    json_decref(root);

    plogf(LOG_INIT, "json_load_commands: loaded %ld commands from %s", commands_list->size, path);
    return true;
}

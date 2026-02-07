/***************************************************************************
 *  JSON Command Serialization - Header                                    *
 *                                                                         *
 *  Handles JSON serialization/deserialization for the command system.     *
 *  Provides load/save functions for commands.json with legacy fallback.   *
 ***************************************************************************/

#ifndef JSON_COMMANDS_H
#define JSON_COMMANDS_H

#include <jansson.h>
#include "../../merc.h"

bool json_load_commands(const char *path);
bool json_save_commands(const char *path);

#endif /* JSON_COMMANDS_H */

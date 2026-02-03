/***************************************************************************
 *  File: json_church.h                                                    *
 *                                                                         *
 *  Church system JSON serialization and deserialization                   *
 *  Converts churches from legacy .org format to JSON format               *
 *                                                                         *
 ***************************************************************************/

#ifndef JSON_CHURCH_H
#define JSON_CHURCH_H

#include <jansson.h>
#include "../../merc.h"

/* Serialize/deserialize entire church */
json_t *json_church_serialize(CHURCH_DATA *church);
CHURCH_DATA *json_church_deserialize(json_t *json);

/* Save/load church JSON files */
bool save_church_json(CHURCH_DATA *church);
bool load_church_json(const char *filename, CHURCH_DATA **church_out);

/* Helper functions for nested structures */
json_t *json_church_rank_serialize(CHURCH_RANK_DATA *rank);
CHURCH_RANK_DATA *json_church_rank_deserialize(json_t *json);

json_t *json_church_member_serialize(CHURCH_PLAYER_DATA *member);
CHURCH_PLAYER_DATA *json_church_member_deserialize(json_t *json);

json_t *json_church_treasure_room_serialize(CHURCH_TREASURE_ROOM *treasure);
CHURCH_TREASURE_ROOM *json_church_treasure_room_deserialize(json_t *json);

#endif /* JSON_CHURCH_H */

/***************************************************************************
 *  Sentience MUD                                                          *
 *  JSON serialization/deserialization for type-specific object data.       *
 *                                                                         *
 *  Provides conversion between type data structs and JSON objects.         *
 *  Used by all save/load paths (text and JSON) for canonical type data.    *
 ***************************************************************************/

#ifndef JSON_OBJ_TYPES_H
#define JSON_OBJ_TYPES_H

#include <jansson.h>

struct obj_data;
struct obj_index_data;

/*
 * Serialize all type-specific data structs on an object to a JSON object.
 * Returns a new json_t* (caller must decref) or NULL if no type data present.
 * Each type is keyed by its name (e.g. "weapon", "armor", "container").
 */
json_t *obj_type_data_to_json(struct obj_data *obj);
json_t *obj_index_type_data_to_json(struct obj_index_data *obj);

/*
 * Deserialize type-specific data from a JSON object into an object's structs.
 * Allocates type structs as needed and sets type_flags bits.
 * Existing type data is freed before being replaced.
 */
void    obj_type_data_from_json(struct obj_data *obj, json_t *json);
void    obj_index_type_data_from_json(struct obj_index_data *obj, json_t *json);

#endif /* JSON_OBJ_TYPES_H */

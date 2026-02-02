#ifndef JSON_RESERVED_H
#define JSON_RESERVED_H

#include <jansson.h>
#include "merc.h"

/* Function declarations for reserved entities JSON handling */

/* Serialization functions */
json_t *json_reserved_serialize(RESERVED_DATA *reserved);

/* Deserialization functions */
RESERVED_DATA *json_reserved_deserialize(json_t *json);

/* Save/Load functions */
bool save_reserved_json(void);
bool load_reserved_json(void);

#endif /* JSON_RESERVED_H */

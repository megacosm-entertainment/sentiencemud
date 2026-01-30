/***************************************************************************
 *  JSON Area Serialization - Header                                       *
 *                                                                          *
 *  Handles JSON serialization/deserialization for area files:             *
 *  - Area metadata and configuration                                      *
 *  - Rooms, mobiles, objects                                              *
 *  - Scripts, resets, exits                                               *
 *  - Wilderness, blueprints, dungeons                                     *
 ***************************************************************************/

#ifndef JSON_AREA_H
#define JSON_AREA_H

#include <jansson.h>
#include "merc.h"

/***************************************************************************
 * Constants                                                               *
 ***************************************************************************/

#define JSON_AREA_SCHEMA_VERSION "1.0.0"

/***************************************************************************
 * Main API Functions                                                      *
 ***************************************************************************/

/* Load area from JSON file */
AREA_DATA *json_area_load(const char *filename);

/* Save area to JSON file */
bool json_area_save(AREA_DATA *area);

/* Serialize area to JSON string (caller must free) */
char *json_area_serialize_to_string(AREA_DATA *area);

/* Fix up shop stock pointers after all areas are loaded */
void fix_shops(void);

/* Convert .are file to JSON (migration tool) */
bool json_area_convert_from_are(const char *are_filename);

/***************************************************************************
 * Serialization Functions (C struct -> JSON)                             *
 ***************************************************************************/

json_t *json_area_serialize_metadata(AREA_DATA *area);
json_t *json_area_serialize_room(ROOM_INDEX_DATA *room);
json_t *json_area_serialize_mobile(MOB_INDEX_DATA *mob);
json_t *json_area_serialize_object(OBJ_INDEX_DATA *obj);
json_t *json_area_serialize_script(SCRIPT_DATA *script);
json_t *json_area_serialize_reset(RESET_DATA *reset);
json_t *json_area_serialize_exit(EXIT_DATA *exit);
json_t *json_area_serialize_shop(SHOP_DATA *shop, AREA_DATA *ref_area);
json_t *json_area_serialize_affect(AFFECT_DATA *affect);
json_t *json_area_serialize_catalyst(AFFECT_DATA *catalyst);
json_t *json_area_serialize_progs(LLIST **progs, AREA_DATA *area);

/***************************************************************************
 * Deserialization Functions (JSON -> C struct)                           *
 ***************************************************************************/

bool json_area_deserialize_metadata(json_t *json, AREA_DATA *area);
ROOM_INDEX_DATA *json_area_deserialize_room(json_t *json, AREA_DATA *area);
MOB_INDEX_DATA *json_area_deserialize_mobile(json_t *json, AREA_DATA *area);
OBJ_INDEX_DATA *json_area_deserialize_object(json_t *json, AREA_DATA *area);
SCRIPT_DATA *json_area_deserialize_script(json_t *json, AREA_DATA *area, int type);
RESET_DATA *json_area_deserialize_reset(json_t *json, AREA_DATA *area);
EXIT_DATA *json_area_deserialize_exit(json_t *json, AREA_DATA *area);
SHOP_DATA *json_area_deserialize_shop(json_t *json, AREA_DATA *area);
SHOP_STOCK_DATA *json_area_deserialize_shop_stock(json_t *json, AREA_DATA *area);
AFFECT_DATA *json_area_deserialize_affect(json_t *json);
AFFECT_DATA *json_area_deserialize_catalyst(json_t *json);
LLIST **json_area_deserialize_progs(json_t *json, AREA_DATA *area, int prog_type);

/* Serialization functions */
json_t *json_area_serialize_shop(SHOP_DATA *shop, AREA_DATA *area);
json_t *json_area_serialize_shop_stock(SHOP_STOCK_DATA *stock, AREA_DATA *area);
json_t *json_area_serialize_progs(LLIST **progs, AREA_DATA *area);

/***************************************************************************
 * Utility Functions                                                       *
 ***************************************************************************/

/* Convert flag array to JSON string array */
json_t *flags_to_json_array(long flags, const struct flag_type *flag_table);

/* Convert JSON string array to flag bitfield */
long json_array_to_flags(json_t *array, const struct flag_type *flag_table);

/* Get string value from JSON with default */
const char *json_get_string_default(json_t *obj, const char *key, const char *default_val);

/* Get integer value from JSON with default */
long json_get_int_default(json_t *obj, const char *key, long default_val);

/* Get boolean value from JSON with default */
bool json_get_bool_default(json_t *obj, const char *key, bool default_val);

/* Index variables serialization */
json_t *json_area_serialize_index_vars(pVARIABLE index_vars, AREA_DATA *area);
pVARIABLE json_area_deserialize_index_vars(json_t *json, AREA_DATA *area);

/* Token serialization */
json_t *json_area_serialize_token(TOKEN_INDEX_DATA *token);
TOKEN_INDEX_DATA *json_area_deserialize_token(json_t *json, AREA_DATA *area);

/* Trade serialization */
json_t *json_area_serialize_trade_list(TRADE_ITEM *trade_list, AREA_DATA *area);
void json_area_deserialize_trade_list(json_t *json, AREA_DATA *area);

#endif /* JSON_AREA_H */

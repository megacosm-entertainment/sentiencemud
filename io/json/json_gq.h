/***************************************************************************
 *  JSON Global Quest Format - GQ Data Serialization with WNUM Support    *
 *                                                                          *
 *  This file contains serialization for global quest data:                *
 *  - GQ mob entries with vnums stored as WNUM                             *
 *  - GQ object entries with vnums stored as WNUM                          *
 *  - Tracking of spawned entities and rewards                             *
 *                                                                          *
 *  Storage: data/gq.json (replaces gq.dat)                                *
 ***************************************************************************/

#ifndef JSON_GQ_H
#define JSON_GQ_H

#include <jansson.h>
#include "../../merc.h"

/* Save global quest data to JSON file */
bool save_gq_json(void);

/* Load global quest data from JSON file */
bool load_gq_json(void);

/* Convert GQ mob data to JSON */
json_t *gq_mob_to_json(GQ_MOB_DATA *gq_mob);

/* Convert JSON to GQ mob data */
GQ_MOB_DATA *json_to_gq_mob(json_t *json);

/* Convert GQ object data to JSON */
json_t *gq_obj_to_json(GQ_OBJ_DATA *gq_obj);

/* Convert JSON to GQ object data */
GQ_OBJ_DATA *json_to_gq_obj(json_t *json);

#endif /* JSON_GQ_H */

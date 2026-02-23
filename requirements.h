#ifndef REQUIREMENTS_H
#define REQUIREMENTS_H

#include <stdbool.h>
#include <jansson.h>

typedef struct char_data CHAR_DATA;
typedef struct mob_index_data MOB_INDEX_DATA;
typedef struct obj_data OBJ_DATA;
typedef struct room_index_data ROOM_INDEX_DATA;
typedef struct token_data TOKEN_DATA;
typedef struct quest_index_v2_data QUEST_INDEX_V2_DATA;

/*
 * REQUIREMENT_CONTEXT carries all the entities relevant to evaluating
 * a prerequisites spec:
 *
 *   actor       — the character being tested (the one who wants to do
 *                 the thing)
 *   self_mob    — the live mob that owns this requirement spec (may be NULL)
 *   self_obj    — the live object instance that owns this requirement spec
 *                 (may be NULL); used as the "self" target for script prog
 *                 evaluation via TRIG_PREWEAR
 *   self_room   — the room that owns this requirement spec (may be NULL)
 *   self_token  — the token that owns this requirement spec (may be NULL)
 *   self_quest  — the quest index that owns this requirement spec (may be
 *                 NULL); used when evaluating prerequisites on a quest
 */
typedef struct requirement_context {
    CHAR_DATA          *actor;
    CHAR_DATA          *self_mob;
    OBJ_DATA           *self_obj;
    ROOM_INDEX_DATA    *self_room;
    TOKEN_DATA         *self_token;
    QUEST_INDEX_V2_DATA *self_quest;
} REQUIREMENT_CONTEXT;

bool requirements_evaluate_json(const json_t *spec,
                                const REQUIREMENT_CONTEXT *context,
                                bool default_if_empty);
bool requirements_evaluate_text(const char *spec_json,
                                const REQUIREMENT_CONTEXT *context,
                                bool default_if_empty);

/*
 * DSL layer: convert builder-friendly text to/from the stored JSON format.
 *
 * requirements_text_to_json  — compile DSL text to JSON string (caller free()s)
 * requirements_json_to_text  — decompile JSON string back to DSL (caller free()s)
 */
char *requirements_text_to_json(const char *text,
                                char *err_buf, size_t err_buf_sz);
char *requirements_json_to_text(const char *json_str);
char *requirements_get_player_string(const char *json_str);
bool  requirements_is_hidden(const char *json_str);

#endif
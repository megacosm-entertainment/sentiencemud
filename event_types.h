#ifndef EVENT_TYPES_H
#define EVENT_TYPES_H

#include "merc.h"

void event_legacy_war_command(CHAR_DATA *ch, char *argument);
void event_legacy_autowar_command(CHAR_DATA *ch, char *argument);
void event_legacy_startinvasion_command(CHAR_DATA *ch, char *argument);
void event_legacy_gq_command(CHAR_DATA *ch, char *argument);
void event_progress_record_kill(CHAR_DATA *killer, CHAR_DATA *victim);
void event_progress_record_collection_turnin(CHAR_DATA *ch, int items_turned);
bool event_progress_complete_invasion_leader(CHAR_DATA *killer, CHAR_DATA *victim);
void event_tag_mobile_spawn(CHAR_DATA *mob, long event_uid, uint32_t instance_id);
void event_tag_object_spawn(OBJ_DATA *obj, long event_uid, uint32_t instance_id);
void event_set_mobile_spawn_bracket(CHAR_DATA *mob, int bracket);
void event_set_object_spawn_bracket(OBJ_DATA *obj, int bracket);
bool event_get_mobile_spawn_source(const CHAR_DATA *mob, long *event_uid, uint32_t *instance_id);
bool event_get_object_spawn_source(const OBJ_DATA *obj, long *event_uid, uint32_t *instance_id);
bool event_get_mobile_spawn_bracket(const CHAR_DATA *mob, int *bracket);
bool event_get_object_spawn_bracket(const OBJ_DATA *obj, int *bracket);
bool event_get_character_active_bracket(const CHAR_DATA *ch, long *event_uid, uint32_t *instance_id, int *bracket);
bool event_runtime_get_source_progress(long event_uid, uint32_t instance_id, int *kills, int *items, int *goal);
bool event_runtime_is_source_leader_phase(long event_uid, uint32_t instance_id, bool *leader_phase);
bool event_runtime_get_source_phase(long event_uid, uint32_t instance_id, char *phase_out, int phase_size);
bool event_runtime_get_progress(const char *event_token, int *kills, int *items, int *goal);
bool event_runtime_is_leader_phase(const char *event_token, bool *leader_phase);
bool event_runtime_get_phase(const char *event_token, char *phase_out, int phase_size);
bool event_runtime_adjust_progress(const char *event_token, int kills_delta, int items_delta);
bool event_runtime_set_goal(const char *event_token, int goal);
bool event_runtime_set_phase(const char *event_token, const char *phase_name);
bool event_runtime_next_phase(const char *event_token);
bool event_runtime_finish(const char *event_token, bool success, const char *reason);

#endif

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
bool event_runtime_get_source_stage_progress(long event_uid, uint32_t instance_id,
	int *stage_index, int *stage_count, int *completion_percent);
bool event_runtime_get_source_objective_progress(long event_uid, uint32_t instance_id,
	int *objectives_met, int *objectives_total, int *objective_percent);
int event_runtime_collect_for_area(const AREA_DATA *area, EVENT_RUNTIME_REF *out, int max_out);
int event_runtime_collect_for_room(const ROOM_INDEX_DATA *room, EVENT_RUNTIME_REF *out, int max_out);
int event_runtime_collect_for_instance(const INSTANCE *instance, EVENT_RUNTIME_REF *out, int max_out);
int event_runtime_collect_for_dungeon(const DUNGEON *dungeon, EVENT_RUNTIME_REF *out, int max_out);
int event_runtime_collect_for_character(const CHAR_DATA *ch, EVENT_RUNTIME_REF *out, int max_out);
int event_runtime_collect_for_object(const OBJ_DATA *obj, EVENT_RUNTIME_REF *out, int max_out);
bool event_runtime_get_progress(const char *event_token, int *kills, int *items, int *goal);
bool event_runtime_is_leader_phase(const char *event_token, bool *leader_phase);
bool event_runtime_get_phase(const char *event_token, char *phase_out, int phase_size);
bool event_runtime_adjust_progress(const char *event_token, int kills_delta, int items_delta);
bool event_runtime_set_goal(const char *event_token, int goal);
bool event_runtime_check_objectives(const char *event_token);
bool event_runtime_set_phase(const char *event_token, const char *phase_name);
bool event_runtime_next_phase(const char *event_token);
bool event_runtime_finish(const char *event_token, bool success, const char *reason);
bool event_runtime_start(const char *event_token, CHAR_DATA *starter, long *event_uid_out, uint32_t *instance_id_out);
bool event_runtime_stop(const char *event_token);
bool event_runtime_get_vars(const char *event_token, pVARIABLE **vars_out);
bool event_runtime_get_vars_by_ref(long event_uid, uint32_t instance_id, pVARIABLE **vars_out);
bool event_runtime_ensure_participation_for_action(CHAR_DATA *ch, long event_uid,
	uint32_t instance_id, const char *action_name, bool notify);
void event_notify_active_events_for_char(CHAR_DATA *ch, bool area_only);
bool event_index_get_vars(const char *event_token, pVARIABLE **vars_out);
bool event_index_get_vars_by_uid(long event_uid, pVARIABLE **vars_out);
const char *event_index_get_name(const EVENT_INDEX_DATA *event_index);
long event_index_get_uid(const EVENT_INDEX_DATA *event_index);
EVENT_INDEX_DATA *get_event_index(long vnum);
EVENT_INDEX_DATA *get_event_index_for_area(AREA_DATA *area, long vnum);
bool event_index_register(EVENT_INDEX_DATA *event_index);
const char *widevnum_string_event(EVENT_INDEX_DATA *event_index, AREA_DATA *pRefArea);

#endif

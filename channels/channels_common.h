/***************************************************************************
 *  channels_common.h — Shared helpers for channel modules                *
 ***************************************************************************/

#ifndef CHANNELS_COMMON_H
#define CHANNELS_COMMON_H

#include "../merc.h"
#include "channel_registry.h"

const char *channel_scope_to_name(CHANNEL_SCOPE scope);
const char *channel_scope_to_display_name(CHANNEL_SCOPE scope);
CHANNEL_SCOPE channel_scope_from_name(const char *name, bool *ok);

long channel_modifier_flag_from_name(const char *name);
const struct flag_type *channel_modifier_flag_table(void);
long channel_text_modifier_mask(void);
const long *channel_text_modifier_fallback_order(size_t *count);

const char *channel_filter_mode_to_name(int mode);
int channel_filter_mode_from_name(const char *name, bool *ok);

bool channel_build_area_scope_topic(AREA_DATA *area, char *topic_buf, size_t topic_buf_sz);
bool channel_build_region_scope_topic(AREA_REGION *region, AREA_DATA *fallback_area,
									  char *topic_buf, size_t topic_buf_sz);
bool channel_build_group_scope_topic(GROUP_DATA *group, char *topic_buf, size_t topic_buf_sz);
bool channel_build_room_scope_topic(ROOM_INDEX_DATA *room, char *topic_buf, size_t topic_buf_sz);
bool channel_build_instance_scope_topic(INSTANCE *instance, char *topic_buf, size_t topic_buf_sz);
bool channel_build_dungeon_scope_topic(DUNGEON *dungeon, char *topic_buf, size_t topic_buf_sz);
bool channel_build_entity_topic(CHAR_DATA *ch, char *topic_buf, size_t topic_buf_sz);
bool channel_build_church_scope_topic(CHURCH_DATA *church, char *topic_buf, size_t topic_buf_sz);

bool channel_topic_expand_pattern(const char *pattern,
								  const CHANNEL_DEF_DATA *def,
								  CHAR_DATA *sender,
								  char *out,
								  size_t out_sz);

#endif /* CHANNELS_COMMON_H */

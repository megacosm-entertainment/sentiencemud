#include "../merc.h"
#include "../tables.h"
#include "channels_common.h"
#include "channel_filter.h"

static const struct flag_type channel_modifier_flags_table[] = {
    { "punctuation_parse", CHANNEL_MOD_PUNCTUATION_PARSE, true, NULL },
    { "emote_strip",       CHANNEL_MOD_EMOTE_STRIP,       true, NULL },
    { "color_strip",       CHANNEL_MOD_COLOR_STRIP,       true, NULL },
    { "caps_normalize",    CHANNEL_MOD_CAPS_NORMALIZE,    true, NULL },
    { "drunk_speech",      CHANNEL_MOD_DRUNK_SPEECH,      true, NULL },
    { NULL,                 0,                              false, NULL }
};

static const long channel_text_modifier_fallback[] = {
    CHANNEL_MOD_EMOTE_STRIP,
    CHANNEL_MOD_COLOR_STRIP,
    CHANNEL_MOD_CAPS_NORMALIZE,
    CHANNEL_MOD_DRUNK_SPEECH
};

const char *channel_scope_to_name(CHANNEL_SCOPE scope)
{
    switch (scope) {
    case CHANNEL_SCOPE_GLOBAL:        return "global";
    case CHANNEL_SCOPE_AREA:          return "area";
    case CHANNEL_SCOPE_REGION:        return "region";
    case CHANNEL_SCOPE_ROOM_WV:       return "room_wv";
    case CHANNEL_SCOPE_DIRECT_ENTITY: return "direct_entity";
    case CHANNEL_SCOPE_GROUP_ID:      return "group_id";
    case CHANNEL_SCOPE_CHURCH_ID:     return "church_id";
    case CHANNEL_SCOPE_INSTANCE_ID:   return "instance_id";
    case CHANNEL_SCOPE_DUNGEON_ID:    return "dungeon_id";
    default:                          return "global";
    }
}

const char *channel_scope_to_display_name(CHANNEL_SCOPE scope)
{
    switch (scope) {
    case CHANNEL_SCOPE_AREA:          return "AREA";
    case CHANNEL_SCOPE_REGION:        return "REGION";
    case CHANNEL_SCOPE_ROOM_WV:       return "ROOM_WV";
    case CHANNEL_SCOPE_DIRECT_ENTITY: return "DIRECT_ENTITY";
    case CHANNEL_SCOPE_GROUP_ID:      return "GROUP_ID";
    case CHANNEL_SCOPE_CHURCH_ID:     return "CHURCH_ID";
    case CHANNEL_SCOPE_INSTANCE_ID:   return "INSTANCE_ID";
    case CHANNEL_SCOPE_DUNGEON_ID:    return "DUNGEON_ID";
    case CHANNEL_SCOPE_GLOBAL:
    default:
        return "GLOBAL";
    }
}

CHANNEL_SCOPE channel_scope_from_name(const char *name, bool *ok)
{
    if (ok)
        *ok = true;

    if (IS_NULLSTR(name)) {
        if (ok)
            *ok = false;
        return CHANNEL_SCOPE_GLOBAL;
    }

    if (!str_cmp(name, "global"))
        return CHANNEL_SCOPE_GLOBAL;
    if (!str_cmp(name, "area"))
        return CHANNEL_SCOPE_AREA;
    if (!str_cmp(name, "region"))
        return CHANNEL_SCOPE_REGION;
    if (!str_cmp(name, "room_wv"))
        return CHANNEL_SCOPE_ROOM_WV;
    if (!str_cmp(name, "direct_entity"))
        return CHANNEL_SCOPE_DIRECT_ENTITY;
    if (!str_cmp(name, "group_id"))
        return CHANNEL_SCOPE_GROUP_ID;
    if (!str_cmp(name, "church_id"))
        return CHANNEL_SCOPE_CHURCH_ID;
    if (!str_cmp(name, "instance_id"))
        return CHANNEL_SCOPE_INSTANCE_ID;
    if (!str_cmp(name, "dungeon_id"))
        return CHANNEL_SCOPE_DUNGEON_ID;

    if (ok)
        *ok = false;
    return CHANNEL_SCOPE_GLOBAL;
}

long channel_modifier_flag_from_name(const char *name)
{
    if (IS_NULLSTR(name))
        return 0;

    if (!str_cmp(name, "punctuation_parse"))
        return CHANNEL_MOD_PUNCTUATION_PARSE;
    if (!str_cmp(name, "emote_strip"))
        return CHANNEL_MOD_EMOTE_STRIP;
    if (!str_cmp(name, "color_strip"))
        return CHANNEL_MOD_COLOR_STRIP;
    if (!str_cmp(name, "caps_normalize"))
        return CHANNEL_MOD_CAPS_NORMALIZE;
    if (!str_cmp(name, "drunk_speech"))
        return CHANNEL_MOD_DRUNK_SPEECH;

    return 0;
}

const struct flag_type *channel_modifier_flag_table(void)
{
    return channel_modifier_flags_table;
}

long channel_text_modifier_mask(void)
{
    return CHANNEL_MOD_EMOTE_STRIP |
           CHANNEL_MOD_COLOR_STRIP |
           CHANNEL_MOD_CAPS_NORMALIZE |
           CHANNEL_MOD_DRUNK_SPEECH;
}

const long *channel_text_modifier_fallback_order(size_t *count)
{
    if (count)
        *count = sizeof(channel_text_modifier_fallback) / sizeof(channel_text_modifier_fallback[0]);

    return channel_text_modifier_fallback;
}

const char *channel_filter_mode_to_name(int mode)
{
    switch (mode) {
    case CHANNEL_FILTER_REDACT: return "redact";
    case CHANNEL_FILTER_BLOCK:  return "block";
    case CHANNEL_FILTER_REVIEW: return "review";
    case CHANNEL_FILTER_ALLOW:
    default:
        return "allow";
    }
}

int channel_filter_mode_from_name(const char *name, bool *ok)
{
    if (ok)
        *ok = true;

    if (IS_NULLSTR(name)) {
        if (ok)
            *ok = false;
        return CHANNEL_FILTER_ALLOW;
    }

    if (!str_cmp(name, "allow"))
        return CHANNEL_FILTER_ALLOW;
    if (!str_cmp(name, "redact"))
        return CHANNEL_FILTER_REDACT;
    if (!str_cmp(name, "block"))
        return CHANNEL_FILTER_BLOCK;
    if (!str_cmp(name, "review"))
        return CHANNEL_FILTER_REVIEW;

    if (ok)
        *ok = false;
    return CHANNEL_FILTER_ALLOW;
}

bool channel_build_area_scope_topic(AREA_DATA *area, char *topic_buf, size_t topic_buf_sz)
{
    if (!area || !topic_buf || topic_buf_sz == 0)
        return false;

    if (!IS_NULLSTR(area->area_topic)) {
        snprintf(topic_buf, topic_buf_sz, "rt:area:%s", area->area_topic);
        return true;
    }

    if (area->uid > 0) {
        snprintf(topic_buf, topic_buf_sz, "rt:area:%ld", area->uid);
        return true;
    }

    return false;
}

bool channel_build_region_scope_topic(AREA_REGION *region, AREA_DATA *fallback_area,
                                      char *topic_buf, size_t topic_buf_sz)
{
    if (!topic_buf || topic_buf_sz == 0)
        return false;

    if (region && !IS_NULLSTR(region->topic)) {
        snprintf(topic_buf, topic_buf_sz, "rt:region:%s", region->topic);
        return true;
    }

    if (region && region->uid > 0) {
        snprintf(topic_buf, topic_buf_sz, "rt:region:%ld", region->uid);
        return true;
    }

    return channel_build_area_scope_topic(fallback_area, topic_buf, topic_buf_sz);
}

bool channel_build_group_scope_topic(GROUP_DATA *group, char *topic_buf, size_t topic_buf_sz)
{
    if (!group || !topic_buf || topic_buf_sz == 0)
        return false;

    if (group->id[0] == 0 && group->id[1] == 0)
        return false;

    snprintf(topic_buf, topic_buf_sz, "rt:group:%lu:%lu", group->id[0], group->id[1]);
    return true;
}

bool channel_build_room_scope_topic(ROOM_INDEX_DATA *room, char *topic_buf, size_t topic_buf_sz)
{
    long wilds_uid;
    long source_vnum;

    if (!room || !topic_buf || topic_buf_sz == 0)
        return false;

    if (room->wilds) {
        wilds_uid = room->w;
        if (wilds_uid <= 0)
            return false;

        snprintf(topic_buf, topic_buf_sz, "rt:room:wv:%ld:%ld:%ld",
                 wilds_uid,
                 room->x,
                 room->y);
        return true;
    }

    if (room->id[0] != 0 || room->id[1] != 0) {
        source_vnum = room->source ? room->source->vnum : room->vnum;
        snprintf(topic_buf, topic_buf_sz, "rt:room:inst:%ld:%lu:%lu",
                 source_vnum,
                 room->id[0],
                 room->id[1]);
        return true;
    }

    snprintf(topic_buf, topic_buf_sz, "rt:room:v:%ld", room->vnum);
    return true;
}

bool channel_build_instance_scope_topic(INSTANCE *instance, char *topic_buf, size_t topic_buf_sz)
{
    ROOM_INDEX_DATA *entrance;
    long source_vnum;

    if (!instance || !topic_buf || topic_buf_sz == 0)
        return false;

    entrance = instance->entrance;
    if (entrance && (entrance->id[0] != 0 || entrance->id[1] != 0)) {
        source_vnum = entrance->source ? entrance->source->vnum : entrance->vnum;
        snprintf(topic_buf, topic_buf_sz, "rt:instance:room:%ld:%lu:%lu",
                 source_vnum,
                 entrance->id[0],
                 entrance->id[1]);
        return true;
    }

    if (instance->dungeon &&
        (instance->dungeon->uid[0] != 0 || instance->dungeon->uid[1] != 0)) {
        snprintf(topic_buf, topic_buf_sz, "rt:instance:dungeon:%lu:%lu:%d",
                 instance->dungeon->uid[0],
                 instance->dungeon->uid[1],
                 instance->floor);
        return true;
    }

    if (entrance) {
        source_vnum = entrance->source ? entrance->source->vnum : entrance->vnum;
        snprintf(topic_buf, topic_buf_sz, "rt:instance:v:%ld:%d", source_vnum, instance->floor);
        return true;
    }

    return false;
}

bool channel_build_dungeon_scope_topic(DUNGEON *dungeon, char *topic_buf, size_t topic_buf_sz)
{
    if (!dungeon || !topic_buf || topic_buf_sz == 0)
        return false;

    if (dungeon->uid[0] == 0 && dungeon->uid[1] == 0)
        return false;

    snprintf(topic_buf, topic_buf_sz, "rt:dungeon:%lu:%lu", dungeon->uid[0], dungeon->uid[1]);
    return true;
}

bool channel_build_entity_topic(CHAR_DATA *ch, char *topic_buf, size_t topic_buf_sz)
{
    if (!ch || !topic_buf || topic_buf_sz == 0)
        return false;
    if (ch->id[0] == 0 && ch->id[1] == 0)
        return false;
    snprintf(topic_buf, topic_buf_sz, "rt:entity:%lu:%lu", ch->id[0], ch->id[1]);
    return true;
}

bool channel_build_church_scope_topic(CHURCH_DATA *church, char *topic_buf, size_t topic_buf_sz)
{
    if (!church || !topic_buf || topic_buf_sz == 0)
        return false;

    if (church->uid <= 0)
        return false;

    snprintf(topic_buf, topic_buf_sz, "rt:church:%ld", church->uid);
    return true;
}

bool channel_topic_expand_pattern(const char *pattern,
                                  const CHANNEL_DEF_DATA *def,
                                  CHAR_DATA *sender,
                                  char *out,
                                  size_t out_sz)
{
    const char *src = pattern;
    char *dst = out;
    char *end = out + out_sz - 1;

    if (!pattern || !def || !out || out_sz == 0)
        return false;

    while (*src && dst < end) {
        if (*src != '$') {
            *dst++ = *src++;
            continue;
        }

        const char *subst = NULL;
        char num[32];
        size_t skip = 0;

        if (strncmp(src, "$channel_id", 11) == 0) {
            subst = def->id;
            skip = 11;
        } else if (strncmp(src, "$area_uid", 9) == 0) {
            skip = 9;
            if (sender && sender->in_room && sender->in_room->area) {
                AREA_DATA *area = sender->in_room->area;
                if (!IS_NULLSTR(area->area_topic))
                    subst = area->area_topic;
                else {
                    snprintf(num, sizeof(num), "%ld", area->uid);
                    subst = num;
                }
            }
        } else if (strncmp(src, "$region_uid", 11) == 0) {
            skip = 11;
            if (sender && sender->in_room) {
                AREA_REGION *region = get_room_region(sender->in_room);
                if (region && !IS_NULLSTR(region->topic))
                    subst = region->topic;
                else if (region && region->uid > 0) {
                    snprintf(num, sizeof(num), "%ld", region->uid);
                    subst = num;
                }
            }
        } else if (strncmp(src, "$group_id1", 10) == 0) {
            skip = 10;
            if (sender && IS_VALID(sender->group)) {
                snprintf(num, sizeof(num), "%lu", sender->group->id[0]);
                subst = num;
            }
        } else if (strncmp(src, "$group_id2", 10) == 0) {
            skip = 10;
            if (sender && IS_VALID(sender->group)) {
                snprintf(num, sizeof(num), "%lu", sender->group->id[1]);
                subst = num;
            }
        } else if (strncmp(src, "$church_uid", 11) == 0) {
            skip = 11;
            if (sender && sender->church) {
                snprintf(num, sizeof(num), "%ld", sender->church->uid);
                subst = num;
            }
        } else if (strncmp(src, "$entity_id1", 11) == 0) {
            skip = 11;
            if (sender) {
                snprintf(num, sizeof(num), "%lu", sender->id[0]);
                subst = num;
            }
        } else if (strncmp(src, "$entity_id2", 11) == 0) {
            skip = 11;
            if (sender) {
                snprintf(num, sizeof(num), "%lu", sender->id[1]);
                subst = num;
            }
        } else {
            *dst++ = *src++;
            continue;
        }

        src += skip;
        if (subst) {
            while (*subst && dst < end)
                *dst++ = *subst++;
        }
    }

    *dst = '\0';
    return dst > out;
}

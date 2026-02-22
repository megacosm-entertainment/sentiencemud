/***************************************************************************
 *  channel_registry.c — Channel definition registry with JSON persistence *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <jansson.h>

#include "merc.h"
#include "channel_registry.h"

/*=========================================================================*
 * Internal state                                                           *
 *=========================================================================*/

#define CHANNEL_REGISTRY_MAX 64

static CHANNEL_DEF_DATA channel_registry[CHANNEL_REGISTRY_MAX];
static int              channel_registry_n           = 0;
static bool             channel_registry_initialized = false;

/*=========================================================================*
 * Built-in defaults                                                        *
 *                                                                         *
 * Loaded by channel_registry_init().  A channels.json on disk replaces   *
 * these when channel_registry_load() is called during startup.            *
 *=========================================================================*/

static const CHANNEL_DEF_DATA channel_defaults[] = {
    /* id        name           command    scope                    plrflags  persistent */
    { "gossip",  "Gossip",      "gossip",  CHANNEL_SCOPE_GLOBAL,        true,  true  },
    { "ooc",     "OOC",         "ooc",     CHANNEL_SCOPE_GLOBAL,        true,  true  },
    { "quote",   "Quote",       "quote",   CHANNEL_SCOPE_GLOBAL,        true,  true  },
    { "flame",   "Flame",       "flame",   CHANNEL_SCOPE_GLOBAL,        true,  true  },
    { "helper",  "Helper",      "helper",  CHANNEL_SCOPE_GLOBAL,        true,  true  },
    { "music",   "Music",       "music",   CHANNEL_SCOPE_GLOBAL,        true,  true  },
    { "immtalk", "Immtalk",     "immtalk", CHANNEL_SCOPE_GLOBAL,        true,  true  },
    { "yell",    "Yell",        "yell",    CHANNEL_SCOPE_AREA,          true,  false },
    { "gtell",   "Group Tell",  "gtell",   CHANNEL_SCOPE_GROUP_ID,      true,  false },
    { "chtalk",  "Church Talk", "chtalk",  CHANNEL_SCOPE_CHURCH_ID,     true,  false },
    { "tell",    "Tell",        "tell",    CHANNEL_SCOPE_DIRECT_ENTITY, false, false },
};

/*=========================================================================*
 * Scope name helpers                                                       *
 *=========================================================================*/

static const char *scope_to_str(CHANNEL_SCOPE scope)
{
    switch (scope) {
    case CHANNEL_SCOPE_GLOBAL:        return "global";
    case CHANNEL_SCOPE_AREA:          return "area";
    case CHANNEL_SCOPE_REGION:        return "region";
    case CHANNEL_SCOPE_ROOM_WV:       return "room_wv";
    case CHANNEL_SCOPE_DIRECT_ENTITY: return "direct_entity";
    case CHANNEL_SCOPE_GROUP_ID:      return "group_id";
    case CHANNEL_SCOPE_CHURCH_ID:     return "church_id";
    default:                          return "global";
    }
}

static CHANNEL_SCOPE scope_from_str(const char *s)
{
    if (!s || !s[0])          return CHANNEL_SCOPE_GLOBAL;
    if (!str_cmp(s, "area"))          return CHANNEL_SCOPE_AREA;
    if (!str_cmp(s, "region"))        return CHANNEL_SCOPE_REGION;
    if (!str_cmp(s, "room_wv"))       return CHANNEL_SCOPE_ROOM_WV;
    if (!str_cmp(s, "direct_entity")) return CHANNEL_SCOPE_DIRECT_ENTITY;
    if (!str_cmp(s, "group_id"))      return CHANNEL_SCOPE_GROUP_ID;
    if (!str_cmp(s, "church_id"))     return CHANNEL_SCOPE_CHURCH_ID;
    return CHANNEL_SCOPE_GLOBAL;
}

/*=========================================================================*
 * Public API                                                               *
 *=========================================================================*/

/**
 * channel_registry_init - Seed the registry with built-in defaults.
 */
bool channel_registry_init(void)
{
    int i;

    if (channel_registry_initialized)
        return true;

    memset(channel_registry, 0, sizeof(channel_registry));
    channel_registry_n = 0;

    for (i = 0; i < (int)(sizeof(channel_defaults) / sizeof(channel_defaults[0])); i++) {
        if (channel_registry_n >= CHANNEL_REGISTRY_MAX)
            break;
        channel_registry[channel_registry_n++] = channel_defaults[i];
    }

    channel_registry_initialized = true;
    log_stringf("ChannelRegistry: initialized with %d built-in definitions",
                channel_registry_n);
    return true;
}

/**
 * channel_registry_load - Load channel definitions from a JSON file.
 *
 * On success, replaces the registry contents completely.
 * On failure, the existing registry (including defaults) is preserved.
 */
bool channel_registry_load(const char *filepath)
{
    json_t        *root, *channels, *entry;
    json_error_t   err;
    CHANNEL_DEF_DATA staging[CHANNEL_REGISTRY_MAX];
    int            staging_n = 0;
    size_t         idx;

    if (!filepath)
        return false;

    root = json_load_file(filepath, 0, &err);
    if (!root) {
        if (err.line > 0)
            log_stringf("ChannelRegistry: cannot load '%s': %s (line %d)",
                        filepath, err.text, err.line);
        else
            log_stringf("ChannelRegistry: '%s' not found, using defaults", filepath);
        return false;
    }

    channels = json_object_get(root, "channels");
    if (!json_is_array(channels)) {
        log_stringf("ChannelRegistry: '%s' missing 'channels' array", filepath);
        json_decref(root);
        return false;
    }

    memset(staging, 0, sizeof(staging));

    json_array_foreach(channels, idx, entry) {
        CHANNEL_DEF_DATA def;
        const char      *s;
        json_t          *v;

        if (staging_n >= CHANNEL_REGISTRY_MAX) {
            log_stringf("ChannelRegistry: registry full (%d max), truncating load",
                        CHANNEL_REGISTRY_MAX);
            break;
        }

        memset(&def, 0, sizeof(def));

        s = json_string_value(json_object_get(entry, "id"));
        if (!s || !s[0])
            continue;
        strlcpy(def.id, s, sizeof(def.id));

        s = json_string_value(json_object_get(entry, "name"));
        strlcpy(def.name, s ? s : def.id, sizeof(def.name));

        s = json_string_value(json_object_get(entry, "command"));
        strlcpy(def.command, s ? s : def.id, sizeof(def.command));

        s = json_string_value(json_object_get(entry, "scope"));
        def.scope = scope_from_str(s);

        v = json_object_get(entry, "allow_player_flags");
        def.allow_player_flags = v ? json_boolean_value(v) : false;

        v = json_object_get(entry, "persistent");
        def.persistent = v ? json_boolean_value(v) : false;

        s = json_string_value(json_object_get(entry, "topic_pattern"));
        if (s) strlcpy(def.topic_pattern, s, sizeof(def.topic_pattern));

        staging[staging_n++] = def;
    }

    json_decref(root);

    /* Commit: replace live registry atomically */
    memcpy(channel_registry, staging, sizeof(staging));
    channel_registry_n = staging_n;

    log_stringf("ChannelRegistry: loaded %d definitions from '%s'",
                channel_registry_n, filepath);
    return true;
}

/**
 * channel_registry_save - Write the current registry to a JSON file.
 */
bool channel_registry_save(const char *filepath)
{
    json_t *root, *channels;
    int     i;

    if (!filepath)
        return false;

    root     = json_object();
    channels = json_array();

    for (i = 0; i < channel_registry_n; i++) {
        const CHANNEL_DEF_DATA *def   = &channel_registry[i];
        json_t                 *entry = json_object();

        json_object_set_new(entry, "id",                  json_string(def->id));
        json_object_set_new(entry, "name",                json_string(def->name));
        json_object_set_new(entry, "command",             json_string(def->command));
        json_object_set_new(entry, "scope",               json_string(scope_to_str(def->scope)));
        json_object_set_new(entry, "allow_player_flags",  json_boolean(def->allow_player_flags));
        json_object_set_new(entry, "persistent",          json_boolean(def->persistent));
        if (def->topic_pattern[0])
            json_object_set_new(entry, "topic_pattern",   json_string(def->topic_pattern));

        json_array_append_new(channels, entry);
    }

    json_object_set_new(root, "version",  json_string("1.0"));
    json_object_set_new(root, "channels", channels);

    if (json_dump_file(root, filepath, JSON_INDENT(2)) != 0) {
        log_stringf("ChannelRegistry: failed to save '%s'", filepath);
        json_decref(root);
        return false;
    }

    json_decref(root);
    log_stringf("ChannelRegistry: saved %d definitions to '%s'",
                channel_registry_n, filepath);
    return true;
}

/**
 * channel_registry_find - Look up a definition by id (case-insensitive).
 */
const CHANNEL_DEF_DATA *channel_registry_find(const char *id)
{
    int i;

    if (IS_NULLSTR(id))
        return NULL;

    for (i = 0; i < channel_registry_n; i++) {
        if (!str_cmp(channel_registry[i].id, id))
            return &channel_registry[i];
    }

    return NULL;
}

/**
 * channel_registry_count - Return the number of definitions in the registry.
 */
int channel_registry_count(void)
{
    return channel_registry_n;
}

/**
 * channel_registry_get - Indexed access into the registry.
 */
const CHANNEL_DEF_DATA *channel_registry_get(int index)
{
    if (index < 0 || index >= channel_registry_n)
        return NULL;
    return &channel_registry[index];
}

/**
 * channel_registry_upsert - Add or overwrite a channel definition.
 */
bool channel_registry_upsert(const CHANNEL_DEF_DATA *def)
{
    int i;

    if (!def || !def->id[0])
        return false;

    for (i = 0; i < channel_registry_n; i++) {
        if (!str_cmp(channel_registry[i].id, def->id)) {
            channel_registry[i] = *def;
            return true;
        }
    }

    if (channel_registry_n >= CHANNEL_REGISTRY_MAX)
        return false;

    channel_registry[channel_registry_n++] = *def;
    return true;
}

/**
 * channel_registry_remove - Remove a definition by id.
 */
bool channel_registry_remove(const char *id)
{
    int i;

    if (IS_NULLSTR(id))
        return false;

    for (i = 0; i < channel_registry_n; i++) {
        if (!str_cmp(channel_registry[i].id, id)) {
            for (; i < channel_registry_n - 1; i++)
                channel_registry[i] = channel_registry[i + 1];
            channel_registry_n--;
            memset(&channel_registry[channel_registry_n], 0,
                   sizeof(channel_registry[0]));
            return true;
        }
    }

    return false;
}

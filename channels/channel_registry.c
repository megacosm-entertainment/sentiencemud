/***************************************************************************
 *  channel_registry.c — Channel definition registry with JSON persistence *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <jansson.h>

#include "../merc.h"
#include "channel_registry.h"
#include "channel_filter.h"
#include "channels_common.h"

/*=========================================================================*
 * Internal state                                                           *
 *=========================================================================*/

#define CHANNEL_REGISTRY_MAX 64

static CHANNEL_DEF_DATA channel_registry[CHANNEL_REGISTRY_MAX];
static int              channel_registry_n           = 0;
static bool             channel_registry_initialized = false;

static void channel_requirements_store_json_value(const json_t *value,
                                                  char *out,
                                                  size_t out_sz)
{
    char *serialized;

    if (!out || out_sz == 0 || !value)
        return;

    if (json_is_string(value)) {
        const char *text = json_string_value(value);
        if (!IS_NULLSTR(text))
            strlcpy(out, text, out_sz);
        return;
    }

    serialized = json_dumps(value, JSON_COMPACT);
    if (!serialized)
        return;

    strlcpy(out, serialized, out_sz);
    free(serialized);
}

static void channel_requirements_emit_json_value(json_t *entry,
                                                 const char *key,
                                                 const char *serialized)
{
    json_t *parsed;
    json_error_t err;

    if (!entry || IS_NULLSTR(key) || IS_NULLSTR(serialized))
        return;

    parsed = json_loads(serialized, 0, &err);
    if (parsed) {
        json_object_set_new(entry, key, parsed);
        return;
    }

    json_object_set_new(entry, key, json_string(serialized));
}

static bool channel_requirements_load_to_wnum(const json_t *value,
                                              WNUM_LOAD *out)
{
    json_t *v;

    if (!out || !value)
        return false;

    out->auid = 0;
    out->vnum = 0;

    if (json_is_integer(value)) {
        out->vnum = (long)json_integer_value(value);
        return out->vnum > 0;
    }

    if (json_is_string(value))
        return parse_widevnum_load(json_string_value(value), out);

    if (!json_is_object(value))
        return false;

    v = json_object_get(value, "wnum");
    if (json_is_string(v) && parse_widevnum_load(json_string_value(v), out))
        return out->vnum > 0;

    v = json_object_get(value, "vnum");
    if (json_is_integer(v))
        out->vnum = (long)json_integer_value(v);
    else if (json_is_string(v)) {
        if (!parse_widevnum_load(json_string_value(v), out))
            return false;
    }

    v = json_object_get(value, "auid");
    if (json_is_integer(v))
        out->auid = (long)json_integer_value(v);

    return out->vnum > 0;
}

static bool channel_requirements_canonicalize_token(json_t *value)
{
    WNUM_LOAD load;
    char wnum_buf[64];

    if (!channel_requirements_load_to_wnum(value, &load))
        return false;

    if (load.auid <= 0) {
        AREA_DATA *area = find_area_by_vnum(load.vnum, NULL);
        if (area)
            load.auid = area->uid;
    }

    if (load.auid > 0)
        snprintf(wnum_buf, sizeof(wnum_buf), "%ld#%ld", load.auid, load.vnum);
    else
        snprintf(wnum_buf, sizeof(wnum_buf), "%ld", load.vnum);

    if (json_is_object(value)) {
        json_object_set_new(value, "wnum", json_string(wnum_buf));
        json_object_del(value, "vnum");
        json_object_del(value, "auid");
        return true;
    }

    return false;
}

static bool channel_requirements_fix_node(json_t *node)
{
    const char *key;
    json_t *value;
    bool changed = false;

    if (!json_is_object(node))
        return false;

    json_object_foreach(node, key, value) {
        if (!str_cmp(key, "all_of") || !str_cmp(key, "any_of")) {
            size_t i;
            json_t *item;

            if (!json_is_array(value))
                continue;

            json_array_foreach(value, i, item) {
                if (channel_requirements_fix_node(item))
                    changed = true;
            }
            continue;
        }

        if (!str_cmp(key, "token")) {
            if (json_is_object(value)) {
                if (channel_requirements_canonicalize_token(value))
                    changed = true;
            } else {
                json_t *replacement = json_object();
                WNUM_LOAD load;
                char wnum_buf[64];

                if (!channel_requirements_load_to_wnum(value, &load))
                    continue;

                if (load.auid <= 0) {
                    AREA_DATA *area = find_area_by_vnum(load.vnum, NULL);
                    if (area)
                        load.auid = area->uid;
                }

                if (load.auid > 0)
                    snprintf(wnum_buf, sizeof(wnum_buf), "%ld#%ld", load.auid, load.vnum);
                else
                    snprintf(wnum_buf, sizeof(wnum_buf), "%ld", load.vnum);

                json_object_set_new(replacement, "wnum", json_string(wnum_buf));
                json_object_set_new(node, key, replacement);
                changed = true;
            }
        }
    }

    return changed;
}

static bool channel_requirements_fix_serialized(char *serialized, size_t out_sz)
{
    json_t *root;
    json_error_t err;
    char *updated;
    bool changed;

    if (IS_NULLSTR(serialized) || out_sz == 0)
        return false;

    root = json_loads(serialized, 0, &err);
    if (!root || !json_is_object(root)) {
        if (root)
            json_decref(root);
        return false;
    }

    changed = channel_requirements_fix_node(root);
    if (!changed) {
        json_decref(root);
        return false;
    }

    updated = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    if (!updated)
        return false;

    strlcpy(serialized, updated, out_sz);
    free(updated);
    return true;
}


/*=========================================================================*
 * Built-in defaults                                                        *
 *                                                                         *
 * Loaded by channel_registry_init().  A channels.json on disk replaces   *
 * these when channel_registry_load() is called during startup.            *
 *=========================================================================*/

static const CHANNEL_DEF_DATA channel_defaults[] = {
        { .id = "gossip",  .name = "Gossip",      .command = "gossip",  .scope = CHANNEL_SCOPE_GLOBAL,
            .allow_player_flags = true,  .persistent = true,  .modifiers = CHANNEL_MOD_DRUNK_SPEECH },
        { .id = "ooc",     .name = "OOC",         .command = "ooc",     .scope = CHANNEL_SCOPE_GLOBAL,
            .allow_player_flags = true,  .persistent = true },
        { .id = "quote",   .name = "Quote",       .command = "quote",   .scope = CHANNEL_SCOPE_GLOBAL,
            .allow_player_flags = true,  .persistent = true },
        { .id = "flame",   .name = "Flame",       .command = "flame",   .scope = CHANNEL_SCOPE_GLOBAL,
            .allow_player_flags = true,  .persistent = true,  .modifiers = CHANNEL_MOD_DRUNK_SPEECH },
        { .id = "helper",  .name = "Helper",      .command = "helper",  .scope = CHANNEL_SCOPE_GLOBAL,
            .allow_player_flags = true,  .persistent = true },
        { .id = "music",   .name = "Music",       .command = "music",   .scope = CHANNEL_SCOPE_GLOBAL,
            .allow_player_flags = true,  .persistent = true,  .modifiers = CHANNEL_MOD_DRUNK_SPEECH },
        { .id = "immtalk", .name = "Immtalk",     .command = "immtalk", .scope = CHANNEL_SCOPE_GLOBAL,
            .allow_player_flags = true,  .persistent = true },
        { .id = "yell",    .name = "Yell",        .command = "yell",    .scope = CHANNEL_SCOPE_AREA,
            .allow_player_flags = true,  .persistent = false, .modifiers = CHANNEL_MOD_DRUNK_SPEECH },
        { .id = "say",     .name = "Say",         .command = "say",     .scope = CHANNEL_SCOPE_ROOM_WV,
            .allow_player_flags = false, .persistent = false,
            .modifiers = CHANNEL_MOD_PUNCTUATION_PARSE | CHANNEL_MOD_DRUNK_SPEECH },
        { .id = "whisper", .name = "Whisper",     .command = "whisper", .scope = CHANNEL_SCOPE_ROOM_WV,
            .allow_player_flags = false, .persistent = false },
        { .id = "sayto",   .name = "Say To",      .command = "sayto",   .scope = CHANNEL_SCOPE_ROOM_WV,
            .allow_player_flags = false, .persistent = false,
            .modifiers = CHANNEL_MOD_PUNCTUATION_PARSE | CHANNEL_MOD_DRUNK_SPEECH },
        { .id = "gtell",   .name = "Group Tell",  .command = "gtell",   .scope = CHANNEL_SCOPE_GROUP_ID,
            .allow_player_flags = true,  .persistent = false },
        { .id = "chtalk",  .name = "Church Talk", .command = "chtalk",  .scope = CHANNEL_SCOPE_CHURCH_ID,
            .allow_player_flags = true,  .persistent = false },
        { .id = "tell",    .name = "Tell",        .command = "tell",    .scope = CHANNEL_SCOPE_DIRECT_ENTITY,
            .allow_player_flags = false, .persistent = false },
        { .id = "announce", .name = "Announcements", .command = "announcements", .scope = CHANNEL_SCOPE_GLOBAL,
            .allow_player_flags = false, .persistent = true },
        { .id = "hints",   .name = "Hints",       .command = "hints",   .scope = CHANNEL_SCOPE_GLOBAL,
            .allow_player_flags = false, .persistent = true },
        { .id = "tells",   .name = "Tells",       .command = "tells",   .scope = CHANNEL_SCOPE_GLOBAL,
            .allow_player_flags = false, .persistent = true },
};

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

        v = json_object_get(entry, "aliases");
        if (v && json_is_array(v)) {
            size_t aidx;
            json_t *alias_value;
            def.alias_count = 0;

            json_array_foreach(v, aidx, alias_value) {
                const char *alias_text = json_string_value(alias_value);
                if (IS_NULLSTR(alias_text) || def.alias_count >= CHANNEL_ALIAS_MAX)
                    continue;

                strlcpy(def.aliases[def.alias_count], alias_text, CHANNEL_ALIAS_LEN);
                def.alias_count++;
            }
        }

        s = json_string_value(json_object_get(entry, "scope"));
        def.scope = channel_scope_from_name(s, NULL);

        v = json_object_get(entry, "allow_player_flags");
        def.allow_player_flags = v ? json_boolean_value(v) : false;

        v = json_object_get(entry, "persistent");
        def.persistent = v ? json_boolean_value(v) : false;

        s = json_string_value(json_object_get(entry, "topic_pattern"));
        if (s) strlcpy(def.topic_pattern, s, sizeof(def.topic_pattern));

        v = json_object_get(entry, "modifiers");
        if (v && json_is_integer(v))
            def.modifiers = (long)json_integer_value(v);

        s = json_string_value(json_object_get(entry, "modifier_order"));
        if (s)
            strlcpy(def.modifier_order, s, sizeof(def.modifier_order));

        v = json_object_get(entry, "channel_flags");
        if (v && json_is_integer(v))
            def.channel_flags = (long)json_integer_value(v);

        v = json_object_get(entry, "filter_enabled");
        if (v && json_is_boolean(v))
            def.filter_enabled = json_boolean_value(v);

        s = json_string_value(json_object_get(entry, "filter_mode"));
        def.filter_mode = channel_filter_mode_from_name(s, NULL);

        s = json_string_value(json_object_get(entry, "filter_simple"));
        if (s)
            strlcpy(def.filter_simple, s, sizeof(def.filter_simple));

        s = json_string_value(json_object_get(entry, "filter_regex"));
        if (s)
            strlcpy(def.filter_regex, s, sizeof(def.filter_regex));

        v = json_object_get(entry, "light_warn_threshold");
        if (v && json_is_integer(v))
            def.light_warn_threshold = (int)json_integer_value(v);

        v = json_object_get(entry, "light_mute_minutes");
        if (v && json_is_integer(v))
            def.light_mute_minutes = (int)json_integer_value(v);

        v = json_object_get(entry, "moderators");
        if (v && json_is_array(v)) {
            size_t midx;
            json_t *mod;
            def.mod_count = 0;

            json_array_foreach(v, midx, mod) {
                const char *mod_name = json_string_value(mod);
                if (!IS_NULLSTR(mod_name) && def.mod_count < CHANNEL_MODERATOR_MAX) {
                    strlcpy(def.moderators[def.mod_count], mod_name,
                            sizeof(def.moderators[def.mod_count]));
                    def.mod_count++;
                }
            }
        }

        v = json_object_get(entry, "review_enabled");
        if (v && json_is_boolean(v))
            def.review_enabled = json_boolean_value(v);

        s = json_string_value(json_object_get(entry, "review_stream"));
        if (s)
            strlcpy(def.review_stream, s, sizeof(def.review_stream));

        s = json_string_value(json_object_get(entry, "fmt_self"));
        if (s)
            strlcpy(def.fmt_self, s, sizeof(def.fmt_self));

        s = json_string_value(json_object_get(entry, "fmt_receiver"));
        if (s)
            strlcpy(def.fmt_receiver, s, sizeof(def.fmt_receiver));

        s = json_string_value(json_object_get(entry, "fmt_notvict"));
        if (s)
            strlcpy(def.fmt_notvict, s, sizeof(def.fmt_notvict));

        v = json_object_get(entry, "history_max_len");
        if (v && json_is_integer(v))
            def.history_max_len = (int)json_integer_value(v);

        v = json_object_get(entry, "history_max_age_seconds");
        if (v && json_is_integer(v))
            def.history_max_age_seconds = (int)json_integer_value(v);

        channel_requirements_store_json_value(json_object_get(entry, "publish_requirements"),
                                              def.publish_requirements,
                                              sizeof(def.publish_requirements));
        channel_requirements_store_json_value(json_object_get(entry, "subscribe_requirements"),
                                              def.subscribe_requirements,
                                              sizeof(def.subscribe_requirements));

        s = json_string_value(json_object_get(entry, "comments"));
        if (s)
            strlcpy(def.comments, s, sizeof(def.comments));

        s = json_string_value(json_object_get(entry, "help_keywords"));
        if (s)
            strlcpy(def.help_keywords, s, sizeof(def.help_keywords));

        s = json_string_value(json_object_get(entry, "summary"));
        if (s)
            strlcpy(def.summary, s, sizeof(def.summary));

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
        {
            json_t *aliases = json_array();
            int a;

            for (a = 0; a < def->alias_count; a++) {
                if (!IS_NULLSTR(def->aliases[a]))
                    json_array_append_new(aliases, json_string(def->aliases[a]));
            }

            json_object_set_new(entry, "aliases", aliases);
        }
        json_object_set_new(entry, "scope",               json_string(channel_scope_to_name(def->scope)));
        json_object_set_new(entry, "allow_player_flags",  json_boolean(def->allow_player_flags));
        json_object_set_new(entry, "persistent",          json_boolean(def->persistent));
        if (def->topic_pattern[0])
            json_object_set_new(entry, "topic_pattern",   json_string(def->topic_pattern));
        if (def->modifiers != 0)
            json_object_set_new(entry, "modifiers",       json_integer(def->modifiers));
        if (def->modifier_order[0])
            json_object_set_new(entry, "modifier_order",  json_string(def->modifier_order));

        if (def->channel_flags != 0)
            json_object_set_new(entry, "channel_flags", json_integer(def->channel_flags));
        json_object_set_new(entry, "filter_enabled", json_boolean(def->filter_enabled));
        json_object_set_new(entry, "filter_mode", json_string(channel_filter_mode_to_name(def->filter_mode)));
        if (def->filter_simple[0])
            json_object_set_new(entry, "filter_simple", json_string(def->filter_simple));
        if (def->filter_regex[0])
            json_object_set_new(entry, "filter_regex", json_string(def->filter_regex));
        json_object_set_new(entry, "light_warn_threshold", json_integer(def->light_warn_threshold));
        json_object_set_new(entry, "light_mute_minutes", json_integer(def->light_mute_minutes));

        {
            json_t *mods = json_array();
            int m;
            for (m = 0; m < def->mod_count; m++)
                json_array_append_new(mods, json_string(def->moderators[m]));
            json_object_set_new(entry, "moderators", mods);
        }

        json_object_set_new(entry, "review_enabled", json_boolean(def->review_enabled));
        json_object_set_new(entry, "review_stream", json_string(def->review_stream));
        if (def->fmt_self[0])
            json_object_set_new(entry, "fmt_self", json_string(def->fmt_self));
        if (def->fmt_receiver[0])
            json_object_set_new(entry, "fmt_receiver", json_string(def->fmt_receiver));
        if (def->fmt_notvict[0])
            json_object_set_new(entry, "fmt_notvict", json_string(def->fmt_notvict));
        json_object_set_new(entry, "history_max_len", json_integer(def->history_max_len));
        json_object_set_new(entry, "history_max_age_seconds", json_integer(def->history_max_age_seconds));
        channel_requirements_emit_json_value(entry,
                                             "publish_requirements",
                                             def->publish_requirements);
        channel_requirements_emit_json_value(entry,
                                             "subscribe_requirements",
                                             def->subscribe_requirements);
        if (def->comments[0])
            json_object_set_new(entry, "comments", json_string(def->comments));
        if (def->help_keywords[0])
            json_object_set_new(entry, "help_keywords", json_string(def->help_keywords));
        if (def->summary[0])
            json_object_set_new(entry, "summary", json_string(def->summary));

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

void channel_registry_fix_requirements(void)
{
    int i;
    int changed = 0;

    for (i = 0; i < channel_registry_n; i++) {
        CHANNEL_DEF_DATA *def = &channel_registry[i];

        if (channel_requirements_fix_serialized(def->publish_requirements,
                                                sizeof(def->publish_requirements)))
            changed++;

        if (channel_requirements_fix_serialized(def->subscribe_requirements,
                                                sizeof(def->subscribe_requirements)))
            changed++;
    }

    if (changed > 0) {
        log_stringf("ChannelRegistry: canonicalized %d token requirement specs", changed);
    }
}

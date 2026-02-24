#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#include <jansson.h>

#include "merc.h"
#include "wilds.h"
#include "wilderness_state.h"
#include "wilderness_storage.h"

typedef struct wilderness_feature_node
{
    WILDERNESS_FEATURE_RECORD record;
    char tile;
    int region;
    int x2;
    int y2;
    struct wilderness_feature_node *next;
} WILDERNESS_FEATURE_NODE;

typedef struct wilderness_actor_node
{
    WILDERNESS_ACTOR_RECORD record;
    struct wilderness_actor_node *next;
} WILDERNESS_ACTOR_NODE;

typedef struct wilderness_state_runtime
{
    long wilds_uid;
    bool dirty;
    bool suppress_dirty;
    int loaded_feature_count;
    int loaded_actor_count;
    time_t dirty_since;
    char dirty_reason[MIL];
    WILDERNESS_FEATURE_NODE *features;
    WILDERNESS_ACTOR_NODE *actors;
    struct wilderness_state_runtime *next;
} WILDERNESS_STATE_RUNTIME;

static bool wilderness_state_ready = false;
static WILDERNESS_STATE_RUNTIME *wilderness_state_runtime_head = NULL;

static bool wilderness_state_file_exists(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0')
        return false;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static size_t wilderness_state_map_len(WILDS_DATA *pWilds)
{
    if (!pWilds || pWilds->map_size_x < 1 || pWilds->map_size_y < 1)
        return 0;

    return (size_t)pWilds->map_size_x * (size_t)pWilds->map_size_y;
}

static void wilderness_state_base_checksum(WILDS_DATA *pWilds, char out_hex[65])
{
    size_t map_len;

    if (!out_hex)
        return;

    map_len = wilderness_state_map_len(pWilds);
    if (!pWilds || !pWilds->staticmap || map_len == 0)
    {
        out_hex[0] = '\0';
        return;
    }

    wilderness_storage_checksum_hex(pWilds->staticmap, map_len, out_hex);
}

static void wilderness_state_free_features(WILDERNESS_FEATURE_NODE *node)
{
    WILDERNESS_FEATURE_NODE *next;

    while (node)
    {
        next = node->next;
        free(node);
        node = next;
    }
}

static void wilderness_state_free_actors(WILDERNESS_ACTOR_NODE *node)
{
    WILDERNESS_ACTOR_NODE *next;

    while (node)
    {
        next = node->next;
        free(node);
        node = next;
    }
}

static void wilderness_state_clear_runtime_payload(WILDERNESS_STATE_RUNTIME *runtime)
{
    if (!runtime)
        return;

    wilderness_state_free_features(runtime->features);
    wilderness_state_free_actors(runtime->actors);
    runtime->features = NULL;
    runtime->actors = NULL;
    runtime->loaded_feature_count = 0;
    runtime->loaded_actor_count = 0;
}

static WILDERNESS_STATE_RUNTIME *wilderness_state_get_runtime(WILDS_DATA *pWilds, bool create)
{
    WILDERNESS_STATE_RUNTIME *runtime;

    if (!pWilds)
        return NULL;

    for (runtime = wilderness_state_runtime_head; runtime; runtime = runtime->next)
        if (runtime->wilds_uid == pWilds->uid)
            return runtime;

    if (!create)
        return NULL;

    runtime = calloc(1, sizeof(*runtime));
    if (!runtime)
        return NULL;

    runtime->wilds_uid = pWilds->uid;
    runtime->next = wilderness_state_runtime_head;
    wilderness_state_runtime_head = runtime;
    return runtime;
}

bool wilderness_state_init(void)
{
    wilderness_state_ready = true;
    return true;
}

void wilderness_state_shutdown(void)
{
    WILDERNESS_STATE_RUNTIME *runtime;
    WILDERNESS_STATE_RUNTIME *next;

    for (runtime = wilderness_state_runtime_head; runtime; runtime = next)
    {
        next = runtime->next;
        wilderness_state_clear_runtime_payload(runtime);
        free(runtime);
    }

    wilderness_state_runtime_head = NULL;
    wilderness_state_ready = false;
}

void wilderness_state_pulse(void)
{
    WILDERNESS_STATE_RUNTIME *runtime;

    if (!wilderness_state_ready)
        return;

    for (runtime = wilderness_state_runtime_head; runtime; runtime = runtime->next)
    {
        if (!runtime->dirty)
            continue;

        if (runtime->dirty_since > 0 && (current_time - runtime->dirty_since) > 300)
        {
            runtime->dirty_since = current_time;
            plogf(LOG_INFO, "wilderness_state: uid %ld remains dirty (%s)", runtime->wilds_uid,
                IS_NULLSTR(runtime->dirty_reason) ? "unspecified" : runtime->dirty_reason);
        }
    }
}

bool wilderness_state_load(WILDS_DATA *pWilds)
{
    WILDERNESS_STATE_RUNTIME *runtime;
    json_error_t err;
    json_t *root;
    json_t *features;
    json_t *actors;
    const char *stored_checksum;
    char current_checksum[65];
    char path[MSL];
    size_t i;

    if (!wilderness_state_ready || !pWilds)
        return false;

    if (!wilderness_storage_build_state_path(pWilds, path, sizeof(path)))
        return false;

    runtime = wilderness_state_get_runtime(pWilds, true);
    if (!runtime)
        return false;

    wilderness_state_clear_runtime_payload(runtime);

    if (!wilderness_state_file_exists(path))
        return true;

    root = json_load_file(path, 0, &err);
    if (!root)
    {
        perrf(LOG_ERROR, "wilderness_state_load: failed to parse %s (%s:%d)", path, err.text, err.line);
        return false;
    }

    if (!json_is_object(root))
    {
        json_decref(root);
        perrf(LOG_ERROR, "wilderness_state_load: invalid root in %s", path);
        return false;
    }

    stored_checksum = json_string_value(json_object_get(root, "base_map_checksum"));
    wilderness_state_base_checksum(pWilds, current_checksum);
    if (!IS_NULLSTR(stored_checksum) && !IS_NULLSTR(current_checksum)
    && str_cmp(stored_checksum, current_checksum) != 0)
    {
        char quarantine_path[MSL];
        size_t base_len = strlen(path);

        if (base_len > sizeof(quarantine_path) - 32)
            base_len = sizeof(quarantine_path) - 32;

        memcpy(quarantine_path, path, base_len);
        quarantine_path[base_len] = '\0';
        snprintf(quarantine_path + base_len,
            sizeof(quarantine_path) - base_len,
            ".quarantine.%ld",
            (long)current_time);
        if (rename(path, quarantine_path) != 0)
        {
            perrf(LOG_ERROR,
                "wilderness_state_load: checksum mismatch for wilds uid %ld and quarantine rename failed (%s)",
                pWilds->uid,
                strerror(errno));
        }
        else
        {
            plogf(LOG_WARN,
                "wilderness_state_load: checksum mismatch for wilds uid %ld, quarantined state file to %s",
                pWilds->uid,
                quarantine_path);
        }

        json_decref(root);
        return true;
    }

    runtime->suppress_dirty = true;

    features = json_object_get(root, "features");
    if (json_is_array(features))
    {
        for (i = 0; i < json_array_size(features); i++)
        {
            json_t *entry = json_array_get(features, i);
            WILDERNESS_FEATURE_NODE *node;
            time_t expires_at;
            int duration;

            if (!json_is_object(entry))
                continue;

            node = calloc(1, sizeof(*node));
            if (!node)
                continue;

            node->record.uid = (long)json_integer_value(json_object_get(entry, "uid"));
            node->record.type = (int)json_integer_value(json_object_get(entry, "type"));
            node->record.x = (int)json_integer_value(json_object_get(entry, "x"));
            node->record.y = (int)json_integer_value(json_object_get(entry, "y"));
            node->record.radius = (int)json_integer_value(json_object_get(entry, "radius"));
            node->record.permanent = json_is_true(json_object_get(entry, "permanent"));
            node->record.created_at = (time_t)json_integer_value(json_object_get(entry, "created_at"));
            node->record.expires_at = (time_t)json_integer_value(json_object_get(entry, "expires_at"));
            node->tile = json_is_string(json_object_get(entry, "tile"))
                ? json_string_value(json_object_get(entry, "tile"))[0]
                : '\0';
            node->region = (int)json_integer_value(json_object_get(entry, "region"));
            node->x2 = (int)json_integer_value(json_object_get(entry, "x2"));
            node->y2 = (int)json_integer_value(json_object_get(entry, "y2"));

            node->next = runtime->features;
            runtime->features = node;
            runtime->loaded_feature_count++;

            if (node->tile == '\0')
                continue;

            expires_at = node->record.expires_at;
            if (expires_at > 0)
            {
                duration = (int)(expires_at - current_time);
                if (duration <= 0)
                    continue;
            }
            else
            {
                duration = 0;
            }

            wilds_add_temporary_zone(pWilds,
                node->record.x,
                node->record.y,
                node->x2 > 0 ? node->x2 : node->record.x,
                node->y2 > 0 ? node->y2 : node->record.y,
                node->tile,
                node->region,
                duration);
        }
    }

    actors = json_object_get(root, "actors");
    if (json_is_array(actors))
    {
        for (i = 0; i < json_array_size(actors); i++)
        {
            json_t *entry = json_array_get(actors, i);
            WILDERNESS_ACTOR_NODE *node;

            if (!json_is_object(entry))
                continue;

            node = calloc(1, sizeof(*node));
            if (!node)
                continue;

            node->record.uid = (long)json_integer_value(json_object_get(entry, "uid"));
            node->record.type = (int)json_integer_value(json_object_get(entry, "type"));
            node->record.x = (int)json_integer_value(json_object_get(entry, "x"));
            node->record.y = (int)json_integer_value(json_object_get(entry, "y"));
            node->record.z = (int)json_integer_value(json_object_get(entry, "z"));
            node->record.heading = (int)json_integer_value(json_object_get(entry, "heading"));
            node->record.speed = (int)json_integer_value(json_object_get(entry, "speed"));
            node->record.region_uid = (long)json_integer_value(json_object_get(entry, "region_uid"));
            node->record.active = json_is_true(json_object_get(entry, "active"));

            node->next = runtime->actors;
            runtime->actors = node;
            runtime->loaded_actor_count++;
        }
    }

    runtime->suppress_dirty = false;
    runtime->dirty = false;
    runtime->dirty_since = 0;
    runtime->dirty_reason[0] = '\0';

    json_decref(root);

    plogf(LOG_DEBUG,
        "wilderness_state_load: loaded features=%d actors=%d for wilds uid %ld",
        runtime->loaded_feature_count,
        runtime->loaded_actor_count,
        pWilds->uid);
    return true;
}

bool wilderness_state_save(WILDS_DATA *pWilds)
{
    WILDERNESS_STATE_RUNTIME *runtime;
    json_t *root;
    json_t *features;
    json_t *actors;
    char checksum[65];
    char path[MSL];
    WILDERNESS_FEATURE_NODE *feature_node;
    WILDERNESS_ACTOR_NODE *actor_node;
    WILDS_CHUNK *chunk;

    if (!wilderness_state_ready || !pWilds)
        return false;

    runtime = wilderness_state_get_runtime(pWilds, true);
    if (!runtime)
        return false;

    if (!runtime->dirty)
        return true;

    root = json_object();
    features = json_array();
    actors = json_array();
    if (!root || !features || !actors)
    {
        if (actors) json_decref(actors);
        if (features) json_decref(features);
        if (root) json_decref(root);
        return false;
    }

    wilderness_state_base_checksum(pWilds, checksum);

    json_object_set_new(root, "schema", json_string("wilderness_state"));
    json_object_set_new(root, "version", json_integer(WILDERNESS_STATE_VERSION));
    json_object_set_new(root, "wilds_uid", json_integer(pWilds->uid));
    json_object_set_new(root, "base_map_checksum", json_string(checksum));
    json_object_set_new(root, "saved_at", json_integer((json_int_t)current_time));

    for (feature_node = runtime->features; feature_node; feature_node = feature_node->next)
    {
        json_t *entry = json_object();
        char tile_str[2] = { feature_node->tile, '\0' };

        if (!entry)
            continue;

        json_object_set_new(entry, "uid", json_integer(feature_node->record.uid));
        json_object_set_new(entry, "type", json_integer(feature_node->record.type));
        json_object_set_new(entry, "x", json_integer(feature_node->record.x));
        json_object_set_new(entry, "y", json_integer(feature_node->record.y));
        json_object_set_new(entry, "x2", json_integer(feature_node->x2));
        json_object_set_new(entry, "y2", json_integer(feature_node->y2));
        json_object_set_new(entry, "radius", json_integer(feature_node->record.radius));
        json_object_set_new(entry, "tile", json_string(tile_str));
        json_object_set_new(entry, "region", json_integer(feature_node->region));
        json_object_set_new(entry, "permanent", feature_node->record.permanent ? json_true() : json_false());
        json_object_set_new(entry, "created_at", json_integer((json_int_t)feature_node->record.created_at));
        json_object_set_new(entry, "expires_at", json_integer((json_int_t)feature_node->record.expires_at));

        json_array_append_new(features, entry);
    }

    for (chunk = pWilds->runtime_chunks; chunk; chunk = chunk->next)
    {
        WILDS_OVERLAY *overlay;

        for (overlay = chunk->overlays; overlay; overlay = overlay->next)
        {
            json_t *entry = json_object();
            char tile_str[2] = { overlay->tile, '\0' };

            if (!entry)
                continue;

            json_object_set_new(entry, "uid", json_integer(overlay->zone_id));
            json_object_set_new(entry, "type", json_integer(WILDERNESS_FEATURE_WEATHER_HAZARD));
            json_object_set_new(entry, "x", json_integer(overlay->x1));
            json_object_set_new(entry, "y", json_integer(overlay->y1));
            json_object_set_new(entry, "x2", json_integer(overlay->x2));
            json_object_set_new(entry, "y2", json_integer(overlay->y2));
            json_object_set_new(entry, "radius", json_integer(0));
            json_object_set_new(entry, "tile", json_string(tile_str));
            json_object_set_new(entry, "region", json_integer(overlay->region));
            json_object_set_new(entry, "permanent", overlay->expires_at <= 0 ? json_true() : json_false());
            json_object_set_new(entry, "created_at", json_integer((json_int_t)current_time));
            json_object_set_new(entry, "expires_at", json_integer((json_int_t)overlay->expires_at));

            json_array_append_new(features, entry);
        }
    }

    for (actor_node = runtime->actors; actor_node; actor_node = actor_node->next)
    {
        json_t *entry = json_object();

        if (!entry)
            continue;

        json_object_set_new(entry, "uid", json_integer(actor_node->record.uid));
        json_object_set_new(entry, "type", json_integer(actor_node->record.type));
        json_object_set_new(entry, "x", json_integer(actor_node->record.x));
        json_object_set_new(entry, "y", json_integer(actor_node->record.y));
        json_object_set_new(entry, "z", json_integer(actor_node->record.z));
        json_object_set_new(entry, "heading", json_integer(actor_node->record.heading));
        json_object_set_new(entry, "speed", json_integer(actor_node->record.speed));
        json_object_set_new(entry, "region_uid", json_integer(actor_node->record.region_uid));
        json_object_set_new(entry, "active", actor_node->record.active ? json_true() : json_false());

        json_array_append_new(actors, entry);
    }

    json_object_set_new(root, "features", features);
    json_object_set_new(root, "actors", actors);

    if (!wilderness_storage_build_state_path(pWilds, path, sizeof(path)))
    {
        json_decref(root);
        return false;
    }

    if (json_dump_file(root, path, JSON_INDENT(2) | JSON_SORT_KEYS) != 0)
    {
        perrf(LOG_ERROR, "wilderness_state_save: failed to write %s (errno=%d)", path, errno);
        json_decref(root);
        return false;
    }

    json_decref(root);

    runtime->dirty = false;
    runtime->dirty_since = 0;
    runtime->dirty_reason[0] = '\0';
    return true;
}

void wilderness_state_mark_dirty(WILDS_DATA *pWilds, const char *reason)
{
    WILDERNESS_STATE_RUNTIME *runtime;

    if (!wilderness_state_ready || !pWilds)
        return;

    runtime = wilderness_state_get_runtime(pWilds, true);
    if (!runtime || runtime->suppress_dirty)
        return;

    runtime->dirty = true;
    if (runtime->dirty_since <= 0)
        runtime->dirty_since = current_time;

    if (!IS_NULLSTR(reason))
        snprintf(runtime->dirty_reason, sizeof(runtime->dirty_reason), "%s", reason);
}

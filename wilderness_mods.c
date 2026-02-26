#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#include <jansson.h>

#include "merc.h"
#include "wilds.h"
#include "wilderness_mods.h"
#include "wilderness_storage.h"

typedef struct wilderness_mods_runtime
{
    long wilds_uid;
    bool dirty;
    bool suppress_dirty;
    int loaded_mod_count;
    time_t dirty_since;
    char dirty_reason[MIL];
    struct wilderness_mods_runtime *next;
} WILDERNESS_MODS_RUNTIME;

static bool wilderness_mods_ready = false;
static WILDERNESS_MODS_RUNTIME *wilderness_mods_runtime_head = NULL;

static bool wilderness_mods_file_exists(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0')
        return false;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static size_t wilderness_mods_map_len(WILDS_DATA *pWilds)
{
    if (!pWilds || pWilds->map_size_x < 1 || pWilds->map_size_y < 1)
        return 0;

    return (size_t)pWilds->map_size_x * (size_t)pWilds->map_size_y;
}

static void wilderness_mods_base_checksum(WILDS_DATA *pWilds, char out_hex[65])
{
    size_t map_len;

    if (!out_hex)
        return;

    map_len = wilderness_mods_map_len(pWilds);
    if (!pWilds || !pWilds->staticmap || map_len == 0)
    {
        out_hex[0] = '\0';
        return;
    }

    wilderness_storage_checksum_hex(pWilds->staticmap, map_len, out_hex);
}

static WILDERNESS_MODS_RUNTIME *wilderness_mods_get_runtime(WILDS_DATA *pWilds, bool create)
{
    WILDERNESS_MODS_RUNTIME *runtime;

    if (!pWilds)
        return NULL;

    for (runtime = wilderness_mods_runtime_head; runtime; runtime = runtime->next)
        if (runtime->wilds_uid == pWilds->uid)
            return runtime;

    if (!create)
        return NULL;

    runtime = calloc(1, sizeof(*runtime));
    if (!runtime)
        return NULL;

    runtime->wilds_uid = pWilds->uid;
    runtime->next = wilderness_mods_runtime_head;
    wilderness_mods_runtime_head = runtime;
    return runtime;
}

bool wilderness_mods_init(void)
{
    wilderness_mods_ready = true;
    return true;
}

void wilderness_mods_shutdown(void)
{
    WILDERNESS_MODS_RUNTIME *runtime;
    WILDERNESS_MODS_RUNTIME *next;

    for (runtime = wilderness_mods_runtime_head; runtime; runtime = next)
    {
        next = runtime->next;
        free(runtime);
    }

    wilderness_mods_runtime_head = NULL;
    wilderness_mods_ready = false;
}

void wilderness_mods_pulse(void)
{
    WILDERNESS_MODS_RUNTIME *runtime;

    if (!wilderness_mods_ready)
        return;

    for (runtime = wilderness_mods_runtime_head; runtime; runtime = runtime->next)
    {
        if (!runtime->dirty)
            continue;

        if (runtime->dirty_since > 0 && (current_time - runtime->dirty_since) > 300)
        {
            runtime->dirty_since = current_time;
            plogf(LOG_INFO, "wilderness_mods: uid %ld remains dirty (%s)", runtime->wilds_uid,
                IS_NULLSTR(runtime->dirty_reason) ? "unspecified" : runtime->dirty_reason);
        }
    }
}

bool wilderness_mods_load(WILDS_DATA *pWilds)
{
    WILDERNESS_MODS_RUNTIME *runtime;
    json_error_t err;
    json_t *root;
    json_t *mods;
    const char *stored_checksum;
    char current_checksum[65];
    char path[MSL];
    size_t i;
    int applied = 0;

    if (!wilderness_mods_ready || !pWilds)
        return false;

    if (!wilderness_storage_build_mods_path(pWilds, path, sizeof(path)))
        return false;

    runtime = wilderness_mods_get_runtime(pWilds, true);
    if (!runtime)
        return false;

    runtime->loaded_mod_count = 0;

    if (!wilderness_mods_file_exists(path))
        return true;

    root = json_load_file(path, 0, &err);
    if (!root)
    {
        perrf(LOG_ERROR, "wilderness_mods_load: failed to parse %s (%s:%d)", path, err.text, err.line);
        return false;
    }

    if (!json_is_object(root))
    {
        json_decref(root);
        perrf(LOG_ERROR, "wilderness_mods_load: invalid root in %s", path);
        return false;
    }

    stored_checksum = json_string_value(json_object_get(root, "base_map_checksum"));
    wilderness_mods_base_checksum(pWilds, current_checksum);
    if (!IS_NULLSTR(stored_checksum) && !IS_NULLSTR(current_checksum)
    && str_cmp(stored_checksum, current_checksum) != 0)
    {
        plogf(LOG_WARN,
            "wilderness_mods_load: checksum mismatch for wilds uid %ld (stored=%s current=%s), skipping apply",
            pWilds->uid,
            stored_checksum,
            current_checksum);
        json_decref(root);
        return true;
    }

    mods = json_object_get(root, "mods");
    if (!json_is_array(mods))
    {
        json_decref(root);
        return true;
    }

    runtime->suppress_dirty = true;
    for (i = 0; i < json_array_size(mods); i++)
    {
        json_t *entry = json_array_get(mods, i);
        json_t *new_tile_json;
        const char *new_tile_str;
        int x;
        int y;

        if (!json_is_object(entry))
            continue;

        x = (int)json_integer_value(json_object_get(entry, "x"));
        y = (int)json_integer_value(json_object_get(entry, "y"));
        new_tile_json = json_object_get(entry, "new_tile");
        new_tile_str = json_is_string(new_tile_json) ? json_string_value(new_tile_json) : NULL;

        if (!new_tile_str || new_tile_str[0] == '\0')
            continue;

        if (set_wilds_runtime_tile(pWilds, x, y, new_tile_str[0]))
            applied++;
    }
    runtime->suppress_dirty = false;

    runtime->loaded_mod_count = applied;
    runtime->dirty = false;
    runtime->dirty_since = 0;
    runtime->dirty_reason[0] = '\0';

    json_decref(root);

    plogf(LOG_DEBUG, "wilderness_mods_load: loaded %d mods for wilds uid %ld", applied, pWilds->uid);
    return true;
}

bool wilderness_mods_save(WILDS_DATA *pWilds)
{
    WILDERNESS_MODS_RUNTIME *runtime;
    json_t *root;
    json_t *mods;
    char checksum[65];
    char path[MSL];
    size_t map_len;
    size_t index;
    int saved_mods = 0;

    if (!wilderness_mods_ready || !pWilds)
        return false;

    map_len = wilderness_mods_map_len(pWilds);
    if (!pWilds->staticmap || !pWilds->map || map_len == 0)
        return true;

    runtime = wilderness_mods_get_runtime(pWilds, true);
    if (!runtime)
        return false;

    root = json_object();
    mods = json_array();
    if (!root || !mods)
    {
        if (mods) json_decref(mods);
        if (root) json_decref(root);
        return false;
    }

    wilderness_mods_base_checksum(pWilds, checksum);

    json_object_set_new(root, "schema", json_string("wilderness_mods"));
    json_object_set_new(root, "version", json_integer(WILDERNESS_MODS_VERSION));
    json_object_set_new(root, "wilds_uid", json_integer(pWilds->uid));
    json_object_set_new(root, "base_map_checksum", json_string(checksum));

    for (index = 0; index < map_len; index++)
    {
        char old_tile = pWilds->staticmap[index];
        char new_tile = pWilds->map[index];

        if (old_tile != new_tile)
        {
            int x = (int)(index % (size_t)pWilds->map_size_x);
            int y = (int)(index / (size_t)pWilds->map_size_x);
            json_t *entry = json_object();
            char old_tile_str[2] = { old_tile, '\0' };
            char new_tile_str[2] = { new_tile, '\0' };

            if (new_tile == '0' && wilds_coord_has_vlink_marker(pWilds, x, y))
                continue;

            if (!entry)
                continue;

            json_object_set_new(entry, "x", json_integer(x));
            json_object_set_new(entry, "y", json_integer(y));
            json_object_set_new(entry, "old_tile", json_string(old_tile_str));
            json_object_set_new(entry, "new_tile", json_string(new_tile_str));
            json_object_set_new(entry, "old_elevation", json_integer(0));
            json_object_set_new(entry, "new_elevation", json_integer(0));
            json_object_set_new(entry, "permanent", json_true());
            json_object_set_new(entry, "authored_by_uid", json_integer(0));
            json_object_set_new(entry, "authored_at", json_integer((json_int_t)current_time));

            json_array_append_new(mods, entry);
            saved_mods++;
        }
    }

    json_object_set_new(root, "mods", mods);

    if (!wilderness_storage_build_mods_path(pWilds, path, sizeof(path)))
    {
        json_decref(root);
        return false;
    }

    if (json_dump_file(root, path, JSON_INDENT(2) | JSON_SORT_KEYS) != 0)
    {
        perrf(LOG_ERROR, "wilderness_mods_save: failed to write %s (errno=%d)", path, errno);
        json_decref(root);
        return false;
    }

    json_decref(root);

    runtime->loaded_mod_count = saved_mods;
    runtime->dirty = false;
    runtime->dirty_since = 0;
    runtime->dirty_reason[0] = '\0';

    plogf(LOG_DEBUG, "wilderness_mods_save: saved %d mods for wilds uid %ld", saved_mods, pWilds->uid);
    return true;
}

void wilderness_mods_mark_dirty(WILDS_DATA *pWilds, const char *reason)
{
    WILDERNESS_MODS_RUNTIME *runtime;

    if (!wilderness_mods_ready || !pWilds)
        return;

    runtime = wilderness_mods_get_runtime(pWilds, true);
    if (!runtime || runtime->suppress_dirty)
        return;

    runtime->dirty = true;
    if (runtime->dirty_since <= 0)
        runtime->dirty_since = current_time;

    if (!IS_NULLSTR(reason))
        snprintf(runtime->dirty_reason, sizeof(runtime->dirty_reason), "%s", reason);
}

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#include <jansson.h>

#include "merc.h"
#include "wilds.h"
#include "wilderness_storage.h"
#include "wilderness_vlinks.h"

extern void free_vlink(WILDS_VLINK *pVLink);

static bool wilderness_vlinks_ready = false;

static json_t *wilderness_vlinks_json_string_safe(const char *value)
{
    return json_string(value ? value : "");
}

static int wilderness_vlinks_json_int_default(json_t *obj, const char *key, int def)
{
    json_t *value;

    if (!obj || !key)
        return def;

    value = json_object_get(obj, key);
    if (!json_is_integer(value))
        return def;

    return (int)json_integer_value(value);
}

static const char *wilderness_vlinks_json_str_default(json_t *obj, const char *key, const char *def)
{
    json_t *value;

    if (!obj || !key)
        return def;

    value = json_object_get(obj, key);
    if (!json_is_string(value))
        return def;

    return json_string_value(value);
}

static bool wilderness_vlinks_file_exists(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0')
        return false;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static WILDS_VLINK *wilderness_vlinks_find_uid(WILDS_DATA *pWilds, long uid)
{
    WILDS_VLINK *vlink;

    if (!pWilds || uid <= 0)
        return NULL;

    for (vlink = pWilds->pVLink; vlink; vlink = vlink->next)
        if (vlink->uid == uid)
            return vlink;

    return NULL;
}

static void wilderness_vlinks_remove_entry(WILDS_DATA *pWilds, WILDS_VLINK *target)
{
    WILDS_VLINK *prev = NULL;
    WILDS_VLINK *iter;

    if (!pWilds || !target)
        return;

    for (iter = pWilds->pVLink; iter; prev = iter, iter = iter->next)
    {
        if (iter != target)
            continue;

        if (iter->current_linkage != VLINK_UNLINKED)
            unlink_vlink(iter);

        if (prev)
            prev->next = iter->next;
        else
            pWilds->pVLink = iter->next;

        iter->next = NULL;
        free_vlink(iter);
        return;
    }
}

static json_t *wilderness_vlink_to_json(WILDS_VLINK *vlink)
{
    json_t *json;

    if (!vlink)
        return NULL;

    json = json_object();
    if (!json)
        return NULL;

    json_object_set_new(json, "uid", json_integer(vlink->uid));
    json_object_set_new(json, "wildsorigin_x", json_integer(vlink->wildsorigin_x));
    json_object_set_new(json, "wildsorigin_y", json_integer(vlink->wildsorigin_y));
    json_object_set_new(json, "door", json_integer(vlink->door));
    json_object_set_new(json, "map_tile", wilderness_vlinks_json_string_safe(vlink->map_tile));

    if (vlink->dest_load.auid > 0 && vlink->dest_load.vnum > 0)
    {
        char wnum_buf[MIL];
        snprintf(wnum_buf, sizeof(wnum_buf), "%ld#%ld", vlink->dest_load.auid, vlink->dest_load.vnum);
        json_object_set_new(json, "destvnum", json_string(wnum_buf));
    }
    else if (vlink->pDestRoom)
    {
        json_object_set_new(json, "destvnum", json_string(widevnum_string_room(vlink->pDestRoom, NULL)));
    }
    else if (vlink->destvnum > 0)
    {
        json_object_set_new(json, "destvnum", json_integer(vlink->destvnum));
    }

    json_object_set_new(json, "destination_mode", json_integer(vlink->destination_mode));
    json_object_set_new(json, "dungeon_floor", json_integer(UMAX(1, vlink->dungeon_floor)));
    json_object_set_new(json, "default_linkage", json_integer(vlink->default_linkage));
    json_object_set_new(json, "current_linkage", json_integer(vlink->current_linkage));
    json_object_set_new(json, "orig_description", wilderness_vlinks_json_string_safe(vlink->orig_description));
    json_object_set_new(json, "orig_keyword", wilderness_vlinks_json_string_safe(vlink->orig_keyword));
    json_object_set_new(json, "orig_rs_flags", json_integer(vlink->orig_rs_flags));
    json_object_set_new(json, "orig_key", json_integer(vlink->orig_key));
    json_object_set_new(json, "orig_lock", json_integer(vlink->orig_lock));
    json_object_set_new(json, "orig_pick", json_integer(vlink->orig_pick));
    json_object_set_new(json, "rev_description", wilderness_vlinks_json_string_safe(vlink->rev_description));
    json_object_set_new(json, "rev_keyword", wilderness_vlinks_json_string_safe(vlink->rev_keyword));
    json_object_set_new(json, "rev_rs_flags", json_integer(vlink->rev_rs_flags));
    json_object_set_new(json, "rev_key", json_integer(vlink->rev_key));
    json_object_set_new(json, "rev_lock", json_integer(vlink->rev_lock));
    json_object_set_new(json, "rev_pick", json_integer(vlink->rev_pick));

    return json;
}

static WILDS_VLINK *wilderness_vlink_from_json(json_t *json, WILDS_DATA *pWilds)
{
    WILDS_VLINK *vlink;
    json_t *destvnum_val;

    if (!json || !pWilds)
        return NULL;

    vlink = new_vlink();
    if (!vlink)
        return NULL;

    vlink->pWilds = pWilds;
    vlink->uid = wilderness_vlinks_json_int_default(json, "uid", 0);
    vlink->wildsorigin_x = wilderness_vlinks_json_int_default(json, "wildsorigin_x", 0);
    vlink->wildsorigin_y = wilderness_vlinks_json_int_default(json, "wildsorigin_y", 0);
    vlink->door = wilderness_vlinks_json_int_default(json, "door", 0);
    free_string(vlink->map_tile);
    vlink->map_tile = str_dup(wilderness_vlinks_json_str_default(json, "map_tile", ""));

    destvnum_val = json_object_get(json, "destvnum");
    if (json_is_string(destvnum_val))
    {
        WNUM_LOAD wload;
        if (parse_widevnum_load(json_string_value(destvnum_val), &wload))
        {
            /*
             * Legacy recovery guard:
             * Some migrated sidecars may encode destvnum as "<wilds_uid>#<vnum>"
             * rather than "<area_uid>#<vnum>". If the parsed area UID does not map
             * to a real area and matches the owning wilds UID, treat it as bare vnum.
             */
            if (wload.auid > 0 && !get_area_from_uid(wload.auid)
                && pWilds && wload.auid == pWilds->uid)
            {
                wload.auid = 0;
            }

            vlink->destvnum = wload.vnum;
            vlink->dest_load = wload;
            vlink->pDestRoom = NULL;
        }
    }
    else
    {
        vlink->destvnum = wilderness_vlinks_json_int_default(json, "destvnum", 0);
        vlink->dest_load.auid = 0;
        vlink->dest_load.vnum = vlink->destvnum;
    }

    vlink->destination_mode = wilderness_vlinks_json_int_default(json, "destination_mode", VLINK_DEST_ROOM);
    vlink->dungeon_floor = UMAX(1, wilderness_vlinks_json_int_default(json, "dungeon_floor", 1));
    vlink->default_linkage = wilderness_vlinks_json_int_default(json, "default_linkage", 0);
    vlink->current_linkage = wilderness_vlinks_json_int_default(json, "current_linkage", 0);
    free_string(vlink->orig_description);
    vlink->orig_description = str_dup(wilderness_vlinks_json_str_default(json, "orig_description", ""));
    free_string(vlink->orig_keyword);
    vlink->orig_keyword = str_dup(wilderness_vlinks_json_str_default(json, "orig_keyword", ""));
    vlink->orig_rs_flags = wilderness_vlinks_json_int_default(json, "orig_rs_flags", 0);
    vlink->orig_key = wilderness_vlinks_json_int_default(json, "orig_key", 0);
    vlink->orig_lock = wilderness_vlinks_json_int_default(json, "orig_lock", 0);
    vlink->orig_pick = wilderness_vlinks_json_int_default(json, "orig_pick", 0);
    free_string(vlink->rev_description);
    vlink->rev_description = str_dup(wilderness_vlinks_json_str_default(json, "rev_description", ""));
    free_string(vlink->rev_keyword);
    vlink->rev_keyword = str_dup(wilderness_vlinks_json_str_default(json, "rev_keyword", ""));
    vlink->rev_rs_flags = wilderness_vlinks_json_int_default(json, "rev_rs_flags", 0);
    vlink->rev_key = wilderness_vlinks_json_int_default(json, "rev_key", 0);
    vlink->rev_lock = wilderness_vlinks_json_int_default(json, "rev_lock", 0);
    vlink->rev_pick = wilderness_vlinks_json_int_default(json, "rev_pick", 0);

    return vlink;
}

bool wilderness_vlinks_init(void)
{
    wilderness_vlinks_ready = true;
    return true;
}

void wilderness_vlinks_shutdown(void)
{
    wilderness_vlinks_ready = false;
}

bool wilderness_vlinks_load(WILDS_DATA *pWilds)
{
    char path[MSL];
    json_error_t err;
    json_t *root;
    json_t *vlinks;
    size_t i;
    int loaded = 0;
    int replaced = 0;

    if (!wilderness_vlinks_ready || !pWilds)
        return false;

    if (!wilderness_storage_build_vlinks_path(pWilds, path, sizeof(path)))
        return false;

    if (!wilderness_vlinks_file_exists(path))
        return true;

    root = json_load_file(path, 0, &err);
    if (!root)
    {
        perrf(LOG_ERROR, "wilderness_vlinks_load: failed to parse %s (%s:%d)", path, err.text, err.line);
        return false;
    }

    if (!json_is_object(root))
    {
        json_decref(root);
        perrf(LOG_ERROR, "wilderness_vlinks_load: invalid root in %s", path);
        return false;
    }

    vlinks = json_object_get(root, "vlinks");
    if (json_is_array(vlinks))
    {
        for (i = 0; i < json_array_size(vlinks); i++)
        {
            json_t *entry = json_array_get(vlinks, i);
            WILDS_VLINK *vlink;
            WILDS_VLINK *existing;
            long uid;

            if (!json_is_object(entry))
                continue;

            uid = (long)json_integer_value(json_object_get(entry, "uid"));
            existing = wilderness_vlinks_find_uid(pWilds, uid);

            vlink = wilderness_vlink_from_json(entry, pWilds);
            if (!vlink)
                continue;

            if (existing)
            {
                wilderness_vlinks_remove_entry(pWilds, existing);
                replaced++;
            }

            add_vlink(pWilds, vlink);
            loaded++;
        }
    }

    json_decref(root);

    if (loaded > 0 || replaced > 0)
    {
        plogf(LOG_INFO,
            "wilderness_vlinks_load: loaded %d vlink(s), replaced %d existing for wilds uid %ld",
            loaded,
            replaced,
            pWilds->uid);
    }

    return true;
}

bool wilderness_vlinks_save(WILDS_DATA *pWilds)
{
    char path[MSL];
    json_t *root;
    json_t *vlinks;
    WILDS_VLINK *vlink;

    if (!wilderness_vlinks_ready || !pWilds)
        return false;

    if (!wilderness_storage_build_vlinks_path(pWilds, path, sizeof(path)))
        return false;

    /*
     * Safety: if a wilderness currently has no vlinks in memory and there is
     * no persisted sidecar yet, do not create a brand-new empty vlinks file.
     * This avoids clobbering recovery opportunities when legacy migration
     * sources are still being restored.
     */
    if (!pWilds->pVLink && !wilderness_vlinks_file_exists(path))
        return true;

    root = json_object();
    vlinks = json_array();
    if (!root || !vlinks)
    {
        if (vlinks) json_decref(vlinks);
        if (root) json_decref(root);
        return false;
    }

    json_object_set_new(root, "schema", json_string("wilderness_vlinks"));
    json_object_set_new(root, "version", json_integer(WILDERNESS_VLINKS_VERSION));
    json_object_set_new(root, "wilds_uid", json_integer(pWilds->uid));

    for (vlink = pWilds->pVLink; vlink; vlink = vlink->next)
    {
        json_t *entry = wilderness_vlink_to_json(vlink);
        if (entry)
            json_array_append_new(vlinks, entry);
    }

    json_object_set_new(root, "vlinks", vlinks);

    if (json_dump_file(root, path, JSON_INDENT(2) | JSON_SORT_KEYS) != 0)
    {
        perrf(LOG_ERROR, "wilderness_vlinks_save: failed to write %s (errno=%d)", path, errno);
        json_decref(root);
        return false;
    }

    json_decref(root);
    return true;
}

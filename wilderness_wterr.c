#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <jansson.h>
#include "io/json/json_common.h"

#include "merc.h"
#include "wilds.h"
#include "wilderness_storage.h"
#include "wilderness_wterr.h"

static bool wilderness_wterr_ready = false;

static bool wilderness_wterr_file_exists(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0')
        return false;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void wilderness_wterr_set_string(char **dst, const char *value)
{
    if (!dst)
        return;

    free_string(*dst);
    *dst = str_dup(value ? value : "");
}

static WILDS_TERRAIN *wilderness_wterr_get_or_create(WILDS_DATA *pWilds, char token)
{
    WILDS_TERRAIN *terrain;

    terrain = get_terrain_by_token(pWilds, token);
    if (terrain)
        return terrain;

    terrain = new_terrain(pWilds);
    if (!terrain)
        return NULL;

    terrain->mapchar = token;
    add_terrain(pWilds, terrain);
    return terrain;
}

static void wilderness_wterr_apply_template_metadata(WILDS_TERRAIN *terrain, json_t *entry)
{
    json_t *value;

    if (!terrain || !terrain->template || !json_is_object(entry))
        return;

    value = json_object_get(entry, "room_flags");
    if (json_is_integer(value))
    {
        long flags = (long)json_integer_value(value);
        terrain->template->rs_room_flag[0] = flags;
        terrain->template->room_flag[0] = flags;
    }

    value = json_object_get(entry, "room2_flags");
    if (json_is_integer(value))
    {
        long flags2 = (long)json_integer_value(value);
        terrain->template->rs_room_flag[1] = flags2;
        terrain->template->room_flag[1] = flags2;
    }

    value = json_object_get(entry, "sector_type");
    if (json_is_integer(value))
    {
        int sector_type = (int)json_integer_value(value);
        room_set_rs_sector_type(terrain->template, sector_type);
        room_set_sector_type(terrain->template, sector_type);
    }
}

bool wilderness_wterr_init(void)
{
    wilderness_wterr_ready = true;
    return true;
}

void wilderness_wterr_shutdown(void)
{
    wilderness_wterr_ready = false;
}

bool wilderness_wterr_load(WILDS_DATA *pWilds)
{
    char path[MSL];
    json_error_t err;
    json_t *root;
    json_t *terrains;
    json_t *default_tile;
    size_t i;

    if (!wilderness_wterr_ready || !pWilds)
        return false;

    if (!wilderness_storage_build_wterr_path(pWilds, path, sizeof(path)))
        return false;

    if (!wilderness_wterr_file_exists(path))
        return true;

    root = json_load_file(path, 0, &err);
    if (!root)
    {
        perrf(LOG_ERROR, "wilderness_wterr_load: failed to parse %s (%s:%d)", path, err.text, err.line);
        return false;
    }

    if (!json_is_object(root))
    {
        json_decref(root);
        perrf(LOG_ERROR, "wilderness_wterr_load: invalid root in %s", path);
        return false;
    }

    default_tile = json_object_get(root, "default_tile");
    if (json_is_string(default_tile) && json_string_length(default_tile) > 0)
        pWilds->cDefaultTerrain = json_string_value(default_tile)[0];

    terrains = json_object_get(root, "terrains");
    if (json_is_array(terrains))
    {
        for (i = 0; i < json_array_size(terrains); i++)
        {
            json_t *entry = json_array_get(terrains, i);
            json_t *tile_json;
            const char *tile;
            WILDS_TERRAIN *terrain;
            json_t *template_vnum_json;
            json_t *template_area_uid_json;

            if (!json_is_object(entry))
                continue;

            tile_json = json_object_get(entry, "tile");
            tile = json_is_string(tile_json) ? json_string_value(tile_json) : NULL;
            if (IS_NULLSTR(tile))
                continue;

            terrain = wilderness_wterr_get_or_create(pWilds, tile[0]);
            if (!terrain)
                continue;

            wilderness_wterr_set_string(&terrain->showchar,
                json_is_string(json_object_get(entry, "showchar"))
                    ? json_get_string(entry, "showchar", "")
                    : "");
            wilderness_wterr_set_string(&terrain->showname,
                json_is_string(json_object_get(entry, "showname"))
                    ? json_get_string(entry, "showname", "")
                    : "");
            wilderness_wterr_set_string(&terrain->briefdesc,
                json_is_string(json_object_get(entry, "briefdesc"))
                    ? json_get_string(entry, "briefdesc", "")
                    : "");

            terrain->nonroom = json_is_true(json_object_get(entry, "nonroom"));
            terrain->wildgen_has_color = json_is_true(json_object_get(entry, "wildgen_has_color"));
            terrain->wildgen_r = (unsigned char)json_integer_value(json_object_get(entry, "wildgen_r"));
            terrain->wildgen_g = (unsigned char)json_integer_value(json_object_get(entry, "wildgen_g"));
            terrain->wildgen_b = (unsigned char)json_integer_value(json_object_get(entry, "wildgen_b"));

            template_vnum_json = json_object_get(entry, "template_vnum");
            template_area_uid_json = json_object_get(entry, "template_area_uid");
            if (json_is_integer(template_vnum_json) && json_is_integer(template_area_uid_json))
            {
                long template_vnum = (long)json_integer_value(template_vnum_json);
                long template_area_uid = (long)json_integer_value(template_area_uid_json);

                if (template_vnum > 0 && template_area_uid > 0)
                {
                    AREA_DATA *template_area = get_area_from_uid(template_area_uid);
                    if (template_area)
                    {
                        ROOM_INDEX_DATA *template_room = get_room_index(template_area, template_vnum);
                        if (template_room)
                        {
                            terrain->template->vnum = template_room->vnum;
                            terrain->template->area = template_room->area;
                            terrain->template->rs_room_flag[0] = template_room->rs_room_flag[0];
                            terrain->template->rs_room_flag[1] = template_room->rs_room_flag[1];
                            terrain->template->room_flag[0] = template_room->room_flag[0];
                            terrain->template->room_flag[1] = template_room->room_flag[1];
                            room_set_rs_sector_type(terrain->template, room_rs_sector_type(template_room));
                            room_set_sector_type(terrain->template, room_sector_type(template_room));
                        }
                    }
                }
            }

            wilderness_wterr_apply_template_metadata(terrain, entry);
        }
    }

    json_decref(root);
    return true;
}

bool wilderness_wterr_save(WILDS_DATA *pWilds)
{
    char path[MSL];
    json_t *root;
    json_t *terrains;
    WILDS_TERRAIN *terrain;
    char default_tile[2] = { '\0', '\0' };

    if (!wilderness_wterr_ready || !pWilds)
        return false;

    if (!wilderness_storage_build_wterr_path(pWilds, path, sizeof(path)))
        return false;

    root = json_object();
    terrains = json_array();
    if (!root || !terrains)
    {
        if (terrains) json_decref(terrains);
        if (root) json_decref(root);
        return false;
    }

    default_tile[0] = pWilds->cDefaultTerrain;

    json_object_set_new(root, "schema", json_string("wilderness_wterr"));
    json_object_set_new(root, "version", json_integer(WILDERNESS_WTERR_VERSION));
    json_object_set_new(root, "wilds_uid", json_integer(pWilds->uid));
    json_object_set_new(root, "default_tile", json_string(default_tile));

    for (terrain = pWilds->pTerrain; terrain; terrain = terrain->next)
    {
        json_t *entry = json_object();
        char tile[2] = { terrain->mapchar, '\0' };

        if (!entry)
            continue;

        json_object_set_new(entry, "tile", json_string(tile));
        json_object_set_new(entry, "showchar", json_string(terrain->showchar ? terrain->showchar : ""));
        json_object_set_new(entry, "showname", json_string(terrain->showname ? terrain->showname : ""));
        json_object_set_new(entry, "briefdesc", json_string(terrain->briefdesc ? terrain->briefdesc : ""));
        json_object_set_new(entry, "nonroom", terrain->nonroom ? json_true() : json_false());
        json_object_set_new(entry, "wildgen_has_color", terrain->wildgen_has_color ? json_true() : json_false());
        json_object_set_new(entry, "wildgen_r", json_integer(terrain->wildgen_r));
        json_object_set_new(entry, "wildgen_g", json_integer(terrain->wildgen_g));
        json_object_set_new(entry, "wildgen_b", json_integer(terrain->wildgen_b));
        json_object_set_new(entry, "room_flags", json_integer(terrain->template ? terrain->template->rs_room_flag[0] : 0));
        json_object_set_new(entry, "room2_flags", json_integer(terrain->template ? terrain->template->rs_room_flag[1] : 0));
        json_object_set_new(entry, "sector_type", json_integer(terrain->template ? room_rs_sector_type(terrain->template) : 0));

        if (terrain->template)
        {
            json_object_set_new(entry, "template_vnum", json_integer(terrain->template->vnum));
            json_object_set_new(entry, "template_area_uid", json_integer(terrain->template->area ? terrain->template->area->uid : 0));
        }

        json_array_append_new(terrains, entry);
    }

    json_object_set_new(root, "terrains", terrains);

    if (json_dump_file(root, path, JSON_INDENT(2) | JSON_SORT_KEYS) != 0)
    {
        perrf(LOG_ERROR, "wilderness_wterr_save: failed to write %s (errno=%d)", path, errno);
        json_decref(root);
        return false;
    }

    json_decref(root);
    return true;
}

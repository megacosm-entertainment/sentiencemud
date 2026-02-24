#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <jansson.h>

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
                    ? json_string_value(json_object_get(entry, "showchar"))
                    : "");
            wilderness_wterr_set_string(&terrain->showname,
                json_is_string(json_object_get(entry, "showname"))
                    ? json_string_value(json_object_get(entry, "showname"))
                    : "");
            wilderness_wterr_set_string(&terrain->briefdesc,
                json_is_string(json_object_get(entry, "briefdesc"))
                    ? json_string_value(json_object_get(entry, "briefdesc"))
                    : "");

            terrain->nonroom = json_is_true(json_object_get(entry, "nonroom"));
            terrain->wildgen_has_color = json_is_true(json_object_get(entry, "wildgen_has_color"));
            terrain->wildgen_r = (unsigned char)json_integer_value(json_object_get(entry, "wildgen_r"));
            terrain->wildgen_g = (unsigned char)json_integer_value(json_object_get(entry, "wildgen_g"));
            terrain->wildgen_b = (unsigned char)json_integer_value(json_object_get(entry, "wildgen_b"));
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

#include <unistd.h>

#include <jansson.h>

#include "../../merc.h"
#include "../../tables.h"
#include "json_common.h"
#include "json_sectors.h"

#define SECTOR_RUNTIME_JSON_FILE SYSTEM_DIR "sectors.json"
#define SECTOR_RUNTIME_JSON_FORMAT "sectors"
#define SECTOR_RUNTIME_JSON_VERSION 1

static json_t *json_sector_hide_messages_serialize(const SECTOR_RUNTIME_DATA *sector)
{
    json_t *array = json_array();
    int i;

    for (i = 0; i < SECTOR_MAX_HIDE_MSGS; i++) {
        if (!IS_NULLSTR(sector->hide_msgs[i]))
            json_array_append_new(array, json_string_safe(sector->hide_msgs[i]));
    }

    return array;
}

static json_t *json_sector_affinities_serialize(const SECTOR_RUNTIME_DATA *sector)
{
    json_t *array = json_array();
    int i;

    for (i = 0; i < SECTOR_MAX_AFFINITIES; i++) {
        if (sector->affinities[i].catalyst > CATALYST_NONE &&
            sector->affinities[i].catalyst < CATALYST_MAX) {
            json_t *entry = json_object();

            json_object_set_new(entry, "type",
                json_string_safe(flag_string(catalyst_types, sector->affinities[i].catalyst)));
            json_object_set_new(entry, "value", json_integer(sector->affinities[i].value));
            json_array_append_new(array, entry);
        }
    }

    return array;
}

static json_t *json_sector_data_serialize(const SECTOR_RUNTIME_DATA *sector)
{
    json_t *obj = json_object();

    json_object_set_new(obj, "id", json_integer(sector->id));
    json_object_set_new(obj, "name", json_string_safe(sector->name));
    json_object_set_new(obj, "description", json_string_safe(sector->description));
    json_object_set_new(obj, "class", json_string_safe(flag_string(sector_class_table(), sector->sector_class)));
    json_object_set_new(obj, "flags", json_integer(sector->flags));
    json_object_set_new(obj, "move_cost", json_integer(sector->move_cost));
    json_object_set_new(obj, "heal_rate", json_integer(sector->heal_rate));
    json_object_set_new(obj, "mana_rate", json_integer(sector->mana_rate));
    json_object_set_new(obj, "move_rate", json_integer(sector->move_rate));
    json_object_set_new(obj, "soil", json_integer(sector->soil));
    json_object_set_new(obj, "comments", json_string_safe(sector->comments));
    json_object_set_new(obj, "hide_messages", json_sector_hide_messages_serialize(sector));
    json_object_set_new(obj, "affinities", json_sector_affinities_serialize(sector));

    return obj;
}

bool json_sector_data_save(const SECTOR_RUNTIME_DATA *sectors, size_t count)
{
    json_t *root;
    json_t *array;
    size_t i;

    if (!sectors || count == 0)
        return false;

    root = json_object();
    array = json_array();

    json_object_set_new(root, "_format", json_string(SECTOR_RUNTIME_JSON_FORMAT));
    json_object_set_new(root, "_version", json_integer(SECTOR_RUNTIME_JSON_VERSION));

    for (i = 0; i < count; i++)
        json_array_append_new(array, json_sector_data_serialize(&sectors[i]));

    json_object_set_new(root, "sectors", array);

    return json_file_save(root, SECTOR_RUNTIME_JSON_FILE, "json_sector_data_save",
        JSON_INDENT(2) | JSON_PRESERVE_ORDER);
}

bool json_sector_data_load(SECTOR_RUNTIME_DATA *sectors, size_t count)
{
    json_t *root;
    json_t *array = NULL;
    size_t index;
    json_t *entry;

    if (!sectors || count == 0)
        return false;

    if (access(SECTOR_RUNTIME_JSON_FILE, F_OK) != 0)
        return false;

    root = json_file_load(SECTOR_RUNTIME_JSON_FILE, "sectors", &array,
        "json_sector_data_load");
    if (!root)
        return false;

    json_array_foreach(array, index, entry) {
        int id;
        const char *name;

        if (!json_is_object(entry))
            continue;

        id = (int)json_get_int(entry, "id", -1);
        if (id < 0 || (size_t)id >= count)
            continue;

        name = json_get_string(entry, "name", sectors[id].name ? sectors[id].name : "unknown");

        free_string(sectors[id].name);
        sectors[id].name = str_dup(name);
        free_string(sectors[id].description);
        sectors[id].description = str_dup(json_get_string(entry, "description",
            sectors[id].description ? sectors[id].description : ""));

        {
            const char *class_name = json_get_string(entry, "class", "none");
            int class_value = flag_value(sector_class_table(), (char *)class_name);
            if (class_value == NO_FLAG)
                class_value = (int)json_get_int(entry, "class_id", sector_class(id));
            sectors[id].sector_class = class_value;
        }

        sectors[id].flags = json_get_int(entry, "flags", sectors[id].flags);
        sectors[id].move_cost = (int)json_get_int(entry, "move_cost", sectors[id].move_cost);
        sectors[id].heal_rate = (int)json_get_int(entry, "heal_rate", sectors[id].heal_rate);
        sectors[id].mana_rate = (int)json_get_int(entry, "mana_rate", sectors[id].mana_rate);
        sectors[id].move_rate = (int)json_get_int(entry, "move_rate", sectors[id].move_rate);
        sectors[id].soil = (int)json_get_int(entry, "soil", sectors[id].soil);

        free_string(sectors[id].comments);
        sectors[id].comments = str_dup(json_get_string(entry, "comments",
            sectors[id].comments ? sectors[id].comments : ""));

        {
            json_t *hide_msgs = json_object_get(entry, "hide_messages");
            int i;

            for (i = 0; i < SECTOR_MAX_HIDE_MSGS; i++) {
                if (sectors[id].hide_msgs[i]) {
                    free_string(sectors[id].hide_msgs[i]);
                    sectors[id].hide_msgs[i] = NULL;
                }
            }

            if (hide_msgs && json_is_array(hide_msgs)) {
                size_t msg_index = 0;
                json_t *msg_entry;

                json_array_foreach(hide_msgs, msg_index, msg_entry) {
                    if (msg_index >= SECTOR_MAX_HIDE_MSGS)
                        break;
                    if (json_is_string(msg_entry))
                        sectors[id].hide_msgs[msg_index] = str_dup(json_string_value(msg_entry));
                }
            }
        }

        {
            json_t *affinities = json_object_get(entry, "affinities");
            int i;

            for (i = 0; i < SECTOR_MAX_AFFINITIES; i++) {
                sectors[id].affinities[i].catalyst = CATALYST_NONE;
                sectors[id].affinities[i].value = 0;
            }

            if (affinities && json_is_array(affinities)) {
                size_t affinity_index = 0;
                json_t *affinity_entry;

                json_array_foreach(affinities, affinity_index, affinity_entry) {
                    const char *type_name;
                    int catalyst;

                    if (affinity_index >= SECTOR_MAX_AFFINITIES)
                        break;
                    if (!json_is_object(affinity_entry))
                        continue;

                    type_name = json_get_string(affinity_entry, "type", "none");
                    catalyst = flag_value(catalyst_types, (char *)type_name);
                    if (catalyst <= CATALYST_NONE || catalyst >= CATALYST_MAX)
                        continue;

                    sectors[id].affinities[affinity_index].catalyst = catalyst;
                    sectors[id].affinities[affinity_index].value =
                        (int)json_get_int(affinity_entry, "value", 0);
                }
            }
        }
    }

    json_decref(root);
    return true;
}

bool json_sector_data_bootstrap_if_missing(const SECTOR_RUNTIME_DATA *sectors, size_t count)
{
    if (access(SECTOR_RUNTIME_JSON_FILE, F_OK) == 0)
        return true;

    return json_sector_data_save(sectors, count);
}
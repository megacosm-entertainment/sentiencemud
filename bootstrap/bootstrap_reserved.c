/**
 * bootstrap_reserved.c - Reserved entity generation for bootstrap
 *
 * Creates all required reserved mobs, objects, and rooms that the game
 * code depends on. These entities are created sequentially in limbo
 * and saved in reserved.json, without relying on hardcoded vnum defines.
 */

#include <stdio.h>
#include <jansson.h>
#include "../merc.h"
#include "bootstrap_internal.h"

/**
 * Reserved entity definition
 */
typedef struct {
    const char *name;
    const char *type;  /* "mob", "obj", "room" */
    bool removable;
    const char *description;
    const char *short_descr;  /* For objects/mobs */
    const char *long_descr;   /* For objects/mobs */
} reserved_entity_t;

/**
 * Required reserved entities from merc.h
 * These MUST exist for the game to function correctly.
 */
static const reserved_entity_t required_entities[] = {
    /* Critical system objects */
    {"obj_silver_one", "obj", false,
        "Single silver coin", "a silver coin", "A single silver coin lies here."},
    {"obj_gold_one", "obj", false,
        "Single gold coin", "a gold coin", "A single gold coin lies here."},
    {"obj_gold_some", "obj", false,
        "Some gold coins", "some gold coins", "Some gold coins lie here."},
    {"obj_silver_some", "obj", false,
        "Some silver coins", "some silver coins", "Some silver coins lie here."},
    {"obj_coins", "obj", false,
        "Coins", "some coins", "Some coins lie here."},

    /* Corpses and body parts */
    {"obj_corpse_npc", "obj", false,
        "NPC corpse prototype", "the corpse of a mob", "The corpse of a mob lies here."},
    {"obj_corpse_pc", "obj", false,
        "PC corpse prototype", "the corpse of a player", "The corpse of a player lies here."},
    {"obj_severed_head", "obj", false,
        "Severed head", "a severed head", "A severed head lies here."},
    {"obj_torn_heart", "obj", false,
        "Torn heart", "a torn heart", "A torn heart lies here."},
    {"obj_sliced_arm", "obj", false,
        "Sliced arm", "a sliced arm", "A sliced arm lies here."},
    {"obj_sliced_leg", "obj", false,
        "Sliced leg", "a sliced leg", "A sliced leg lies here."},
    {"obj_guts", "obj", false,
        "Guts", "some guts", "Some guts lie here."},
    {"obj_brains", "obj", false,
        "Brains", "some brains", "Some brains lie here."},

    /* Common items */
    {"obj_mushroom", "obj", false,
        "Magic mushroom", "a magic mushroom", "A magic mushroom grows here."},
    {"OBJ_VNUM_LIGHT_BALL", "obj", false,
        "Ball of light", "a ball of light", "A ball of light floats here."},
    {"obj_spring", "obj", false,
        "Water spring", "a spring", "A spring bubbles here."},
    {"OBJ_VNUM_DISC", "obj", false,
        "Floating disc", "a floating disc", "A floating disc hovers here."},
    {"obj_portal", "obj", false,
        "Portal", "a portal", "A shimmering portal hangs in the air."},
    {"obj_nav_chart", "obj", false,
        "Navigational chart", "a navigational chart", "A navigational chart lies here."},
    {"obj_map", "obj", false,
        "Map", "a map", "A map lies here."},

    /* Dummy object for testing */
    {"obj_dummy", "obj", false,
        "Dummy object", "a dummy object", "A dummy object lies here."},

    /* Pit object */
    {"obj_pit", "obj", false,
        "Pit", "a pit", "A pit yawns before you."},

    /* Abyss items */
    {"obj_portal_abyss", "obj", false,
        "Abyss portal", "an abyss portal", "An abyss portal shimmers here."},
    {"OBJ_VNUM_KEY_ABYSS", "obj", false,
        "Abyss key", "an abyss key", "An abyss key lies here."},

    /* Orb/hammer items */
    {"OBJ_VNUM_CURSED_ORB", "obj", false,
        "Cursed orb", "a cursed orb", "A cursed orb pulses with dark energy."},
    {"OBJ_VNUM_GLASS_HAMMER", "obj", false,
        "Glass hammer", "a glass hammer", "A fragile glass hammer lies here."},
    {"OBJ_VNUM_GOLD_WHISTLE", "obj", false,
        "Gold whistle", "a gold whistle", "A gold whistle lies here."},

    /* Crafting/spell items */
    {"obj_empty_vial", "obj", false,
        "Empty vial", "an empty vial", "An empty vial lies here."},
    {"obj_potion", "obj", false,
        "Potion", "a potion", "A potion lies here."},
    {"obj_blank_scroll", "obj", false,
        "Blank scroll", "a blank scroll", "A blank scroll lies here."},
    {"obj_scroll", "obj", false,
        "Scroll", "a scroll", "A scroll lies here."},

    /* Quest items */
    {"obj_quest_scroll", "obj", false,
        "Quest scroll", "a quest scroll", "A quest scroll lies here."},
    {"obj_mordrake_crystal_ball", "obj", false,
        "Crystal ball", "a crystal ball", "A crystal ball rests here."},
    {"obj_alemnos_armor", "obj", false,
        "Alemnos armor", "an ornate armor", "An ornate suit of armor rests here."},

    /* Special effect objects */
    {"obj_bomb_smoke", "obj", false,
        "Smoke bomb", "a smoke bomb", "A smoke bomb lies here."},
    {"obj_cloud_stinking", "obj", false,
        "Stinking cloud", "a stinking cloud", "A stinking cloud fills the area."},
    {"obj_spell_spelltrap", "obj", false,
        "Spell trap", "a spell trap", "A spell trap lies here."},
    {"obj_cloud_withering", "obj", false,
        "Withering cloud", "a withering cloud", "A withering cloud fills the area."},
    {"obj_spell_icestorm", "obj", false,
        "Ice storm", "an ice storm", "An ice storm rages here."},
    {"obj_spell_darkness", "obj", false,
        "Magical darkness", "magical darkness", "Magical darkness fills the room."},
    {"obj_spell_roomshield", "obj", false,
        "Room shield", "a protective shield", "A protective shield covers the room."},

    /* Relics */
    {"OBJ_VNUM_RELIC_EXTRA_DAMAGE", "obj", false,
        "Relic of power", "a relic of power", "A relic of power lies here."},
    {"OBJ_VNUM_RELIC_EXTRA_XP", "obj", false,
        "Relic of knowledge", "a relic of knowledge", "A relic of knowledge lies here."},
    {"OBJ_VNUM_RELIC_EXTRA_PNEUMA", "obj", false,
        "Relic of soul", "a relic of soul", "A relic of soul lies here."},
    {"OBJ_VNUM_RELIC_HP_REGEN", "obj", false,
        "Relic of health", "a relic of health", "A relic of health lies here."},
    {"OBJ_VNUM_RELIC_MANA_REGEN", "obj", false,
        "Relic of magic", "a relic of magic", "A relic of magic lies here."},

    /* Newbie equipment */
    {"obj_newb_quarterstaff", "obj", false,
        "Newbie quarterstaff", "a simple quarterstaff", "A simple quarterstaff lies here."},
    {"obj_newb_dagger", "obj", false,
        "Newbie dagger", "a simple dagger", "A simple dagger lies here."},
    {"obj_newb_sword", "obj", false,
        "Newbie sword", "a simple sword", "A simple sword lies here."},
    {"obj_newb_armour", "obj", false,
        "Newbie armour", "simple leather armour", "Simple leather armour lies here."},
    {"obj_newb_cloak", "obj", false,
        "Newbie cloak", "a simple cloak", "A simple cloak lies here."},
    {"obj_newb_leggings", "obj", false,
        "Newbie leggings", "simple leggings", "Simple leggings lie here."},
    {"obj_newb_boots", "obj", false,
        "Newbie boots", "simple boots", "Simple boots lie here."},
    {"obj_newb_helm", "obj", false,
        "Newbie helm", "a simple helm", "A simple helm lies here."},

    /* Reserved mobs */
    {"mob_abyss_gatekeeper", "mob", false,
        "Abyss gatekeeper", "the abyss gatekeeper", "The abyss gatekeeper stands here."},

    /* Reserved rooms */
    {"ROOM_VNUM_ABYSS_GATE", "room", false,
        "Abyss gate room", NULL, NULL},
    {"room_default_recall", "room", false,
        "Temple room", NULL, NULL},
    {"ROOM_VNUM_PLITH_AIRSHIP", "room", false,
        "Plith airship room", NULL, NULL},
    {"room_begin_new_character", "room", false,
        "School room", NULL, NULL},
    {"room_chat_lobby", "room", false,
        "Chat room", NULL, NULL},
    {"room_limbo", "room", false,
        "Limbo room", NULL, NULL},
    {"room_plith_harbour", "room", false,
        "Plith harbour room", NULL, NULL},
    {"room_southern_harbour", "room", false,
        "Southern harbour room", NULL, NULL},
    {"room_northern_harbour", "room", false,
        "Northern harbour room", NULL, NULL},

    /* End marker */
    {NULL, NULL, false, NULL, NULL, NULL}
};

typedef struct {
    const char *name;
    long area_uid;
    long vnum;
    bool removable;
    const char *description;
} reserved_dungeon_t;

static const reserved_dungeon_t required_dungeons[] = {
    {"maze_death", 1299, 1, false, "Geldoff's Maze dungeon"},
    {"maze_poa",   1557, 1, false, "Pyramid of the Abyss dungeon"},
    {NULL, 0, 0, false, NULL}
};

/**
 * create_reserved_entities - Generate reserved entities in limbo area
 *
 * Creates minimal prototypes for all required mobs, objects, and rooms
 * in the limbo area (UID 1) and adds them to the reserved.json list.
 *
 * @return true on success
 */
bool create_reserved_entities(void)
{
    json_t *limbo_root;
    json_t *limbo_objs;
    json_t *limbo_mobs;
    json_t *reserved_root;
    json_t *reserved_entities;
    json_error_t error;

    /* Load existing limbo.json */
    limbo_root = json_load_file("area/limbo.json", 0, &error);
    if (!limbo_root) {
        fprintf(stderr, "Failed to load limbo.json: %s\n", error.text);
        return false;
    }

    /* Get or create objects and mobs arrays */
    limbo_objs = json_object_get(limbo_root, "objects");
    if (!limbo_objs) {
        limbo_objs = json_array();
        json_object_set_new(limbo_root, "objects", limbo_objs);
    }

    limbo_mobs = json_object_get(limbo_root, "mobiles");
    if (!limbo_mobs) {
        limbo_mobs = json_array();
        json_object_set_new(limbo_root, "mobiles", limbo_mobs);
    }

    /* Create reserved.json structure */
    reserved_root = json_object();
    json_object_set_new(reserved_root, "version", json_integer(1));
    reserved_entities = json_array();

    /* Generate each required entity */
    int obj_count = 0;
    int mob_count = 0;
    int room_count = 0;
    int dungeon_count = 0;
    int obj_vnum = 0;
    int mob_vnum = 0;
    int room_vnum = 0;

    if (limbo_objs && json_is_array(limbo_objs)) {
        size_t idx;
        json_t *elem;
        json_array_foreach(limbo_objs, idx, elem) {
            json_t *vnum_val = json_object_get(elem, "vnum");
            if (vnum_val && json_is_integer(vnum_val)) {
                int vnum = json_integer_value(vnum_val);
                if (vnum > obj_vnum) obj_vnum = vnum;
            }
        }
    }

    for (int i = 0; required_dungeons[i].name != NULL; i++) {
        const reserved_dungeon_t *dng = &required_dungeons[i];

        json_t *reserved_entry = json_object();
        json_object_set_new(reserved_entry, "name", json_string(dng->name));
        json_object_set_new(reserved_entry, "type", json_string("dungeon"));
        json_object_set_new(reserved_entry, "area_uid", json_integer(dng->area_uid));
        json_object_set_new(reserved_entry, "vnum", json_integer(dng->vnum));
        json_object_set_new(reserved_entry, "removable", json_boolean(dng->removable));
        json_object_set_new(reserved_entry, "description", json_string(dng->description));
        json_array_append_new(reserved_entities, reserved_entry);
        dungeon_count++;
    }

    if (limbo_mobs && json_is_array(limbo_mobs)) {
        size_t idx;
        json_t *elem;
        json_array_foreach(limbo_mobs, idx, elem) {
            json_t *vnum_val = json_object_get(elem, "vnum");
            if (vnum_val && json_is_integer(vnum_val)) {
                int vnum = json_integer_value(vnum_val);
                if (vnum > mob_vnum) mob_vnum = vnum;
            }
        }
    }

    for (int i = 0; required_entities[i].name != NULL; i++) {
        const reserved_entity_t *ent = &required_entities[i];

        /* Add to reserved list */
        int vnum = 0;
        if (strcmp(ent->type, "obj") == 0) {
            vnum = ++obj_vnum;
        } else if (strcmp(ent->type, "mob") == 0) {
            vnum = ++mob_vnum;
        } else if (strcmp(ent->type, "room") == 0) {
            vnum = ++room_vnum;
        }
        json_t *reserved_entry = json_object();
        json_object_set_new(reserved_entry, "name", json_string(ent->name));
        json_object_set_new(reserved_entry, "type", json_string(ent->type));
        json_object_set_new(reserved_entry, "area_uid", json_integer(1));  /* Limbo */
        json_object_set_new(reserved_entry, "vnum", json_integer(vnum));
        json_object_set_new(reserved_entry, "removable", json_boolean(ent->removable));
        json_object_set_new(reserved_entry, "description", json_string(ent->description));
        json_array_append_new(reserved_entities, reserved_entry);

        /* Create entity prototype in limbo area */
        if (strcmp(ent->type, "obj") == 0) {
            json_t *obj = json_object();
            json_object_set_new(obj, "vnum", json_integer(vnum));
            json_object_set_new(obj, "name", json_string(ent->short_descr));
            json_object_set_new(obj, "short_descr", json_string(ent->short_descr));
            json_object_set_new(obj, "long_descr", json_string(ent->long_descr));
            json_object_set_new(obj, "description", json_string("Bootstrap-generated required object."));
            json_object_set_new(obj, "item_type", json_string("trash"));
            json_object_set_new(obj, "level", json_integer(1));
            json_object_set_new(obj, "weight", json_integer(1));
            json_object_set_new(obj, "cost", json_integer(0));
            json_array_append_new(limbo_objs, obj);
            obj_count++;
        } else if (strcmp(ent->type, "mob") == 0) {
            json_t *mob = json_object();
            json_object_set_new(mob, "vnum", json_integer(vnum));
            json_object_set_new(mob, "name", json_string(ent->short_descr));
            json_object_set_new(mob, "short_descr", json_string(ent->short_descr));
            json_object_set_new(mob, "long_descr", json_string(ent->long_descr));
            json_object_set_new(mob, "description", json_string("Bootstrap-generated required mobile."));
            json_object_set_new(mob, "level", json_integer(1));
            json_object_set_new(mob, "race", json_string("human"));
            json_array_append_new(limbo_mobs, mob);
            mob_count++;
        } else if (strcmp(ent->type, "room") == 0) {
            /* Rooms are handled separately */
            room_count++;
        }
    }

    /* Save updated limbo.json */
    if (json_dump_file(limbo_root, "area/limbo.json", JSON_INDENT(2)) != 0) {
        fprintf(stderr, "Failed to save updated limbo.json\n");
        json_decref(limbo_root);
        json_decref(reserved_root);
        return false;
    }
    json_decref(limbo_root);

    /* Save reserved.json */
    json_object_set_new(reserved_root, "count", json_integer(obj_count + mob_count + room_count + dungeon_count));
    json_object_set_new(reserved_root, "entities", reserved_entities);

    if (json_dump_file(reserved_root, "data/system/reserved.json", JSON_INDENT(2)) != 0) {
        fprintf(stderr, "Failed to save reserved.json\n");
        json_decref(reserved_root);
        return false;
    }
    json_decref(reserved_root);

    printf("  Created %d objects, %d mobs, %d rooms in limbo\n", obj_count, mob_count, room_count);
    printf("  Added %d entities to reserved list\n", obj_count + mob_count + room_count + dungeon_count);

    return true;
}

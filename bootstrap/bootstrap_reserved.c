/**
 * bootstrap_reserved.c - Reserved entity generation for bootstrap
 *
 * Creates all required reserved mobs, objects, and rooms that the game
 * code depends on. These entities are defined by constants in merc.h
 * and must exist for the game to function properly.
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
    int vnum;
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
    {"obj_silver_one", "obj", OBJ_VNUM_SILVER_ONE, false,
        "Single silver coin", "a silver coin", "A single silver coin lies here."},
    {"obj_gold_one", "obj", OBJ_VNUM_GOLD_ONE, false,
        "Single gold coin", "a gold coin", "A single gold coin lies here."},
    {"obj_gold_some", "obj", OBJ_VNUM_GOLD_SOME, false,
        "Some gold coins", "some gold coins", "Some gold coins lie here."},
    {"obj_silver_some", "obj", OBJ_VNUM_SILVER_SOME, false,
        "Some silver coins", "some silver coins", "Some silver coins lie here."},
    {"obj_coins", "obj", OBJ_VNUM_COINS, false,
        "Coins", "some coins", "Some coins lie here."},

    /* Corpses and body parts */
    {"obj_corpse_npc", "obj", OBJ_VNUM_CORPSE_NPC, false,
        "NPC corpse prototype", "the corpse of a mob", "The corpse of a mob lies here."},
    {"obj_corpse_pc", "obj", OBJ_VNUM_CORPSE_PC, false,
        "PC corpse prototype", "the corpse of a player", "The corpse of a player lies here."},
    {"obj_severed_head", "obj", OBJ_VNUM_SEVERED_HEAD, false,
        "Severed head", "a severed head", "A severed head lies here."},
    {"obj_torn_heart", "obj", OBJ_VNUM_TORN_HEART, false,
        "Torn heart", "a torn heart", "A torn heart lies here."},
    {"obj_sliced_arm", "obj", OBJ_VNUM_SLICED_ARM, false,
        "Sliced arm", "a sliced arm", "A sliced arm lies here."},
    {"obj_sliced_leg", "obj", OBJ_VNUM_SLICED_LEG, false,
        "Sliced leg", "a sliced leg", "A sliced leg lies here."},
    {"obj_guts", "obj", OBJ_VNUM_GUTS, false,
        "Guts", "some guts", "Some guts lie here."},
    {"obj_brains", "obj", OBJ_VNUM_BRAINS, false,
        "Brains", "some brains", "Some brains lie here."},

    /* Common items */
    {"obj_mushroom", "obj", OBJ_VNUM_MUSHROOM, false,
        "Magic mushroom", "a magic mushroom", "A magic mushroom grows here."},
    {"obj_light_ball", "obj", OBJ_VNUM_LIGHT_BALL, false,
        "Ball of light", "a ball of light", "A ball of light floats here."},
    {"obj_spring", "obj", OBJ_VNUM_SPRING, false,
        "Water spring", "a spring", "A spring bubbles here."},
    {"obj_disc", "obj", OBJ_VNUM_DISC, false,
        "Floating disc", "a floating disc", "A floating disc hovers here."},
    {"obj_portal", "obj", OBJ_VNUM_PORTAL, false,
        "Portal", "a portal", "A shimmering portal hangs in the air."},
    {"obj_navigational_chart", "obj", OBJ_VNUM_NAVIGATIONAL_CHART, false,
        "Navigational chart", "a navigational chart", "A navigational chart lies here."},

    /* Dummy object for testing */
    {"obj_dummy", "obj", OBJ_VNUM_DUMMY, false,
        "Dummy object", "a dummy object", "A dummy object lies here."},

    /* Pit object */
    {"obj_pit", "obj", OBJ_VNUM_PIT, false,
        "Pit", "a pit", "A pit yawns before you."},

    /* Crafting/spell items */
    {"obj_empty_vial", "obj", OBJ_VNUM_EMPTY_VIAL, false,
        "Empty vial", "an empty vial", "An empty vial lies here."},
    {"obj_potion", "obj", OBJ_VNUM_POTION, false,
        "Potion", "a potion", "A potion lies here."},
    {"obj_blank_scroll", "obj", OBJ_VNUM_BLANK_SCROLL, false,
        "Blank scroll", "a blank scroll", "A blank scroll lies here."},
    {"obj_scroll", "obj", OBJ_VNUM_SCROLL, false,
        "Scroll", "a scroll", "A scroll lies here."},

    /* Quest items */
    {"obj_quest_scroll", "obj", OBJ_VNUM_QUEST_SCROLL, false,
        "Quest scroll", "a quest scroll", "A quest scroll lies here."},

    /* Special effect objects */
    {"obj_smoke_bomb", "obj", OBJ_VNUM_SMOKE_BOMB, false,
        "Smoke bomb", "a smoke bomb", "A smoke bomb lies here."},
    {"obj_stinking_cloud", "obj", OBJ_VNUM_STINKING_CLOUD, false,
        "Stinking cloud", "a stinking cloud", "A stinking cloud fills the area."},
    {"obj_spell_trap", "obj", OBJ_VNUM_SPELL_TRAP, false,
        "Spell trap", "a spell trap", "A spell trap lies here."},
    {"obj_withering_cloud", "obj", OBJ_VNUM_WITHERING_CLOUD, false,
        "Withering cloud", "a withering cloud", "A withering cloud fills the area."},
    {"obj_ice_storm", "obj", OBJ_VNUM_ICE_STORM, false,
        "Ice storm", "an ice storm", "An ice storm rages here."},
    {"obj_room_darkness", "obj", OBJ_VNUM_ROOM_DARKNESS, false,
        "Magical darkness", "magical darkness", "Magical darkness fills the room."},
    {"obj_roomshield", "obj", OBJ_VNUM_ROOMSHIELD, false,
        "Room shield", "a protective shield", "A protective shield covers the room."},

    /* Newbie equipment */
    {"obj_newb_quarterstaff", "obj", OBJ_VNUM_NEWB_QUARTERSTAFF, false,
        "Newbie quarterstaff", "a simple quarterstaff", "A simple quarterstaff lies here."},
    {"obj_newb_dagger", "obj", OBJ_VNUM_NEWB_DAGGER, false,
        "Newbie dagger", "a simple dagger", "A simple dagger lies here."},
    {"obj_newb_sword", "obj", OBJ_VNUM_NEWB_SWORD, false,
        "Newbie sword", "a simple sword", "A simple sword lies here."},
    {"obj_newb_armour", "obj", OBJ_VNUM_NEWB_ARMOUR, false,
        "Newbie armour", "simple leather armour", "Simple leather armour lies here."},
    {"obj_newb_cloak", "obj", OBJ_VNUM_NEWB_CLOAK, false,
        "Newbie cloak", "a simple cloak", "A simple cloak lies here."},
    {"obj_newb_leggings", "obj", OBJ_VNUM_NEWB_LEGGINGS, false,
        "Newbie leggings", "simple leggings", "Simple leggings lie here."},
    {"obj_newb_boots", "obj", OBJ_VNUM_NEWB_BOOTS, false,
        "Newbie boots", "simple boots", "Simple boots lie here."},
    {"obj_newb_helm", "obj", OBJ_VNUM_NEWB_HELM, false,
        "Newbie helm", "a simple helm", "A simple helm lies here."},

    /* End marker */
    {NULL, NULL, 0, false, NULL, NULL, NULL}
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

    for (int i = 0; required_entities[i].name != NULL; i++) {
        const reserved_entity_t *ent = &required_entities[i];

        /* Add to reserved list */
        json_t *reserved_entry = json_object();
        json_object_set_new(reserved_entry, "name", json_string(ent->name));
        json_object_set_new(reserved_entry, "type", json_string(ent->type));
        json_object_set_new(reserved_entry, "area_uid", json_integer(1));  /* Limbo */
        json_object_set_new(reserved_entry, "vnum", json_integer(ent->vnum));
        json_object_set_new(reserved_entry, "removable", json_boolean(ent->removable));
        json_object_set_new(reserved_entry, "description", json_string(ent->description));
        json_array_append_new(reserved_entities, reserved_entry);

        /* Create entity prototype in limbo area */
        if (strcmp(ent->type, "obj") == 0) {
            json_t *obj = json_object();
            json_object_set_new(obj, "vnum", json_integer(ent->vnum));
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
            json_object_set_new(mob, "vnum", json_integer(ent->vnum));
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
    json_object_set_new(reserved_root, "count", json_integer(obj_count + mob_count + room_count));
    json_object_set_new(reserved_root, "entities", reserved_entities);

    if (json_dump_file(reserved_root, "data/system/reserved.json", JSON_INDENT(2)) != 0) {
        fprintf(stderr, "Failed to save reserved.json\n");
        json_decref(reserved_root);
        return false;
    }
    json_decref(reserved_root);

    printf("  Created %d objects, %d mobs, %d rooms in limbo\n", obj_count, mob_count, room_count);
    printf("  Added %d entities to reserved list\n", obj_count + mob_count + room_count);

    return true;
}

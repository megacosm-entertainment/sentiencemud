/*
 * gmcp_sentience.c — Sentience-native GMCP packages
 *
 * Implements Sentience.Char.* and Sentience.Room.* packages.
 * JSON built via Jansson; dirty-flag caching per descriptor.
 *
 * See docs/PLAN_GMCP_REWORK.md for the full package specification.
 */

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "merc.h"
#include "wilds.h"
#include "gmcp_sentience.h"
#include "protocol.h"
#include "class_data.h"
#include "log.h"
#include "account/preferences.h"
#include "utils/buffer.h"

/* ── Pure JSON builders ─────────────────────────────────────────────
 *
 *  These take explicit parameters and return a new json_t* object.
 *  Caller must json_decref() the result.  No game-state dependencies,
 *  making them directly testable.
 * ─────────────────────────────────────────────────────────────────── */

json_t *sentience_build_vitals_json(long hp, long max_hp,
                                     long mana, long max_mana,
                                     long move, long max_move)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "hp",       json_integer(hp));
    json_object_set_new(obj, "hp_max",   json_integer(max_hp));
    json_object_set_new(obj, "mana",     json_integer(mana));
    json_object_set_new(obj, "mana_max", json_integer(max_mana));
    json_object_set_new(obj, "move",     json_integer(move));
    json_object_set_new(obj, "move_max", json_integer(max_move));
    json_object_set_new(obj, "_v",       json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_stats_json(int str, int int_, int wis, int dex, int con,
                                    int str_perm, int int_perm, int wis_perm,
                                    int dex_perm, int con_perm,
                                    int hitroll, int damroll, int wimpy)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "str",      json_integer(str));
    json_object_set_new(obj, "str_base", json_integer(str_perm));
    json_object_set_new(obj, "int",      json_integer(int_));
    json_object_set_new(obj, "int_base", json_integer(int_perm));
    json_object_set_new(obj, "wis",      json_integer(wis));
    json_object_set_new(obj, "wis_base", json_integer(wis_perm));
    json_object_set_new(obj, "dex",      json_integer(dex));
    json_object_set_new(obj, "dex_base", json_integer(dex_perm));
    json_object_set_new(obj, "con",      json_integer(con));
    json_object_set_new(obj, "con_base", json_integer(con_perm));
    json_object_set_new(obj, "hitroll",  json_integer(hitroll));
    json_object_set_new(obj, "damroll",  json_integer(damroll));
    json_object_set_new(obj, "wimpy",    json_integer(wimpy));
    json_object_set_new(obj, "_v",       json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_combat_json(int ac_pierce, int ac_bash,
                                     int ac_slash, int ac_exotic)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "ac_pierce", json_integer(ac_pierce));
    json_object_set_new(obj, "ac_bash",   json_integer(ac_bash));
    json_object_set_new(obj, "ac_slash",  json_integer(ac_slash));
    json_object_set_new(obj, "ac_exotic", json_integer(ac_exotic));
    json_object_set_new(obj, "_v",        json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_worth_json(int alignment, long xp, long xp_tnl,
                                    int practices, long gold)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "alignment", json_integer(alignment));
    json_object_set_new(obj, "xp",        json_integer(xp));
    json_object_set_new(obj, "xp_tnl",    json_integer(xp_tnl));
    json_object_set_new(obj, "practices", json_integer(practices));
    json_object_set_new(obj, "gold",      json_integer(gold));
    json_object_set_new(obj, "_v",        json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_identity_json(const sentience_identity_input_t *data)
{
    json_t *obj;
    json_t *classes;
    int i;

    if (!data) return NULL;

    obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "name",      json_string(data->name ? data->name : ""));
    json_object_set_new(obj, "race_wnum", json_string(data->race_wnum ? data->race_wnum : ""));
    json_object_set_new(obj, "race",      json_string(data->race_name ? data->race_name : ""));
    json_object_set_new(obj, "body_type", json_string(data->body_type ? data->body_type : "neutral"));
    json_object_set_new(obj, "level",     json_integer(data->level));
    json_object_set_new(obj, "tot_level", json_integer(data->tot_level));
    json_object_set_new(obj, "title",     json_string(data->title ? data->title : ""));

    classes = json_array();
    for (i = 0; i < data->num_classes; i++) {
        json_t *cls = json_object();
        json_object_set_new(cls, "id",         json_string(data->classes[i].id ? data->classes[i].id : ""));
        json_object_set_new(cls, "name",       json_string(data->classes[i].name ? data->classes[i].name : ""));
        json_object_set_new(cls, "level",      json_integer(data->classes[i].level));
        json_object_set_new(cls, "is_primary", data->classes[i].is_primary ? json_true() : json_false());
        json_array_append_new(classes, cls);
    }
    json_object_set_new(obj, "classes", classes);
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_room_json(const sentience_room_input_t *data)
{
    json_t *obj;
    json_t *exits;
    int i;

    if (!data) return NULL;

    obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "wnum",      json_string(data->wnum ? data->wnum : ""));
    json_object_set_new(obj, "name",      json_string(data->name ? data->name : ""));
    json_object_set_new(obj, "area_name", json_string(data->area_name ? data->area_name : ""));
    json_object_set_new(obj, "area_wnum", json_string(data->area_wnum ? data->area_wnum : ""));
    json_object_set_new(obj, "sector",    json_string(data->sector ? data->sector : ""));

    exits = json_object();
    for (i = 0; i < data->num_exits; i++) {
        json_t *ex = json_object();
        json_object_set_new(ex, "wnum",      json_string(data->exits[i].wnum ? data->exits[i].wnum : ""));
        json_object_set_new(ex, "name",      json_string(data->exits[i].name ? data->exits[i].name : ""));
        json_object_set_new(ex, "is_door",   data->exits[i].is_door ? json_true() : json_false());
        json_object_set_new(ex, "is_closed", data->exits[i].is_closed ? json_true() : json_false());
        json_object_set_new(ex, "is_locked", data->exits[i].is_locked ? json_true() : json_false());
        json_object_set_new(exits, data->exits[i].dir ? data->exits[i].dir : "unknown", ex);
    }
    json_object_set_new(obj, "exits", exits);

    if (data->is_wilds) {
        json_object_set_new(obj, "is_wilds", json_true());
        json_object_set_new(obj, "wilds_uid", json_integer(data->wilds_uid));
        json_object_set_new(obj, "wilds_x",   json_integer(data->wilds_x));
        json_object_set_new(obj, "wilds_y",   json_integer(data->wilds_y));
    } else {
        json_object_set_new(obj, "is_wilds", json_false());
    }

    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_room_map(const sentience_room_map_input_t *input)
{
    json_t *obj;

    if (!input || !input->type || !input->map_text)
        return NULL;

    obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "_v",       json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "type",     json_string(input->type));
    json_object_set_new(obj, "map_text", json_string(input->map_text));
    json_object_set_new(obj, "width",    json_integer(input->width));
    json_object_set_new(obj, "height",   json_integer(input->height));

    return obj;
}

/* ── Sentience.Channel.Message builder ─────────────────────────── */

json_t *sentience_build_channel_message(const sentience_channel_message_input_t *input)
{
    json_t *obj;

    if (!input || !input->channel || !input->text)
        return NULL;

    obj = json_object();
    json_object_set_new(obj, "_v",        json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "channel",   json_string(input->channel));
    json_object_set_new(obj, "sender",    json_string(input->sender ? input->sender : ""));
    json_object_set_new(obj, "text",      json_string(input->text));
    json_object_set_new(obj, "timestamp", json_integer(input->timestamp));

    if (input->tell_target)
        json_object_set_new(obj, "tell_target", json_string(input->tell_target));

    return obj;
}

/* ── Phase 3: Pure JSON builders ────────────────────────────────── */

json_t *sentience_build_client_ready_capabilities_json(void)
{
    json_t *obj = json_object();
    json_t *packages = json_array();
    json_t *features = json_array();

    if (!obj) return NULL;

    json_array_append_new(packages, json_string("Sentience.Char.Identity"));
    json_array_append_new(packages, json_string("Sentience.Char.Vitals"));
    json_array_append_new(packages, json_string("Sentience.Char.Stats"));
    json_array_append_new(packages, json_string("Sentience.Char.Combat"));
    json_array_append_new(packages, json_string("Sentience.Char.Worth"));
    json_array_append_new(packages, json_string("Sentience.Char.Affects"));
    json_array_append_new(packages, json_string("Sentience.Char.Enemies"));
    json_array_append_new(packages, json_string("Sentience.Room.Info"));
    json_array_append_new(packages, json_string("Sentience.Room.Contents"));
    json_array_append_new(packages, json_string("Sentience.Room.Map"));
    json_array_append_new(packages, json_string("Sentience.Channel.Message"));
    json_array_append_new(packages, json_string("Sentience.Client.Preferences"));
    json_array_append_new(packages, json_string("Sentience.Link"));

    json_array_append_new(features, json_string("links"));
    json_array_append_new(features, json_string("osc8"));

    json_object_set_new(obj, "packages", packages);
    json_object_set_new(obj, "features", features);
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_client_ready_state_json(int tick_rate, int pulse_per_second)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "tick_rate",        json_integer(tick_rate));
    json_object_set_new(obj, "pulse_per_second", json_integer(pulse_per_second));
    json_object_set_new(obj, "_v",               json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_affects_json(const sentience_affect_input_t *affects, int num_affects)
{
    json_t *obj = json_object();
    json_t *arr = json_array();
    int i;

    if (!obj) return NULL;

    for (i = 0; i < num_affects; i++) {
        json_t *af = json_object();

        json_object_set_new(af, "name",
            json_string(affects[i].name ? affects[i].name : "unknown"));

        if (affects[i].wnum)
            json_object_set_new(af, "wnum", json_string(affects[i].wnum));
        else
            json_object_set_new(af, "wnum", json_null());

        json_object_set_new(af, "duration",          json_integer(affects[i].duration));
        json_object_set_new(af, "estimated_seconds",  json_integer(affects[i].estimated_seconds));

        if (affects[i].modifier)
            json_object_set_new(af, "modifier", json_string(affects[i].modifier));
        else
            json_object_set_new(af, "modifier", json_null());

        json_object_set_new(af, "level", json_integer(affects[i].level));

        json_array_append_new(arr, af);
    }

    json_object_set_new(obj, "affects", arr);
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_enemies_json(const sentience_enemy_input_t *enemies, int num_enemies,
                                      long self_hp, long self_max_hp)
{
    json_t *obj = json_object();
    json_t *arr = json_array();
    int i;

    if (!obj) return NULL;

    for (i = 0; i < num_enemies; i++) {
        json_t *en = json_object();
        json_t *iid = json_array();

        json_object_set_new(en, "name",
            json_string(enemies[i].name ? enemies[i].name : "someone"));

        json_array_append_new(iid, json_integer(enemies[i].instance_id[0]));
        json_array_append_new(iid, json_integer(enemies[i].instance_id[1]));
        json_object_set_new(en, "instance_id", iid);

        json_object_set_new(en, "hp_pct",     json_integer(enemies[i].hp_pct));
        json_object_set_new(en, "is_primary",  enemies[i].is_primary ? json_true() : json_false());
        json_object_set_new(en, "target",
            json_string(enemies[i].target ? enemies[i].target : "someone"));

        json_array_append_new(arr, en);
    }

    json_object_set_new(obj, "enemies", arr);

    if (num_enemies > 0) {
        json_t *self = json_object();
        long hp_pct = (self_hp * 100) / UMAX(1, self_max_hp);

        json_object_set_new(self, "hp",     json_integer(self_hp));
        json_object_set_new(self, "max_hp", json_integer(self_max_hp));
        json_object_set_new(self, "hp_pct", json_integer(hp_pct));
        json_object_set_new(obj, "self", self);
    } else {
        json_object_set_new(obj, "self", json_null());
    }

    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_room_contents_json(const sentience_room_contents_input_t *data)
{
    json_t *obj;
    json_t *items, *npcs_arr, *players_arr, *doors_arr;
    int i;

    if (!data) return NULL;

    obj = json_object();
    if (!obj) return NULL;

    items = json_array();
    for (i = 0; i < data->num_items; i++) {
        json_t *it = json_object();
        json_t *iid = json_array();

        json_object_set_new(it, "name",
            json_string(data->items[i].name ? data->items[i].name : "something"));
        json_array_append_new(iid, json_integer(data->items[i].instance_id[0]));
        json_array_append_new(iid, json_integer(data->items[i].instance_id[1]));
        json_object_set_new(it, "instance_id", iid);
        json_object_set_new(it, "short_desc",
            json_string(data->items[i].short_desc ? data->items[i].short_desc : ""));
        json_array_append_new(items, it);
    }
    json_object_set_new(obj, "items", items);

    npcs_arr = json_array();
    for (i = 0; i < data->num_npcs; i++) {
        json_t *npc = json_object();
        json_t *iid = json_array();

        json_object_set_new(npc, "name",
            json_string(data->npcs[i].name ? data->npcs[i].name : "someone"));
        json_array_append_new(iid, json_integer(data->npcs[i].instance_id[0]));
        json_array_append_new(iid, json_integer(data->npcs[i].instance_id[1]));
        json_object_set_new(npc, "instance_id", iid);
        json_object_set_new(npc, "short_desc",
            json_string(data->npcs[i].short_desc ? data->npcs[i].short_desc : ""));
        json_array_append_new(npcs_arr, npc);
    }
    json_object_set_new(obj, "npcs", npcs_arr);

    players_arr = json_array();
    for (i = 0; i < data->num_players; i++) {
        json_t *pl = json_object();
        json_object_set_new(pl, "name",
            json_string(data->players[i].name ? data->players[i].name : "someone"));
        json_array_append_new(players_arr, pl);
    }
    json_object_set_new(obj, "players", players_arr);

    doors_arr = json_array();
    for (i = 0; i < data->num_doors; i++) {
        json_t *dr = json_object();
        json_object_set_new(dr, "direction",
            json_string(data->doors[i].direction ? data->doors[i].direction : "unknown"));
        json_object_set_new(dr, "state",
            json_string(data->doors[i].state ? data->doors[i].state : "open"));
        json_object_set_new(dr, "is_locked",
            data->doors[i].is_locked ? json_true() : json_false());
        json_array_append_new(doors_arr, dr);
    }
    json_object_set_new(obj, "doors", doors_arr);

    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

/* ── Helpers for game-loop integration ──────────────────────────── */

/*
 * Serialize JSON and send as a GMCP package via the protocol layer.
 */
void sentience_send_package(descriptor_t *d, const char *package, json_t *json)
{
    char *dump;

    if (!d || !json)
        return;

    dump = json_dumps(json, JSON_COMPACT);
    if (dump) {
        SendGMCPRaw(d, package, dump);
        free(dump);
    }

    json_decref(json);
}

/**
 * sentience_send_client_preferences - Send current GMCP prefs to client
 *
 * Sends Sentience.Client.Preferences with the current effective values.
 * Called on login and after a client preference update.
 */
void sentience_send_client_preferences(descriptor_t *d)
{
    json_t *obj;
    CHAR_DATA *ch;

    if (!d || !d->character)
        return;

    ch = d->character;

    obj = json_object();
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "gmcp_channels",
                        json_boolean(pref_gmcp_channels(ch)));
    json_object_set_new(obj, "gmcp_suppress_channels",
                        json_boolean(pref_gmcp_suppress_channels(ch)));
    json_object_set_new(obj, "gmcp_suppress_minimap",
                        json_boolean(pref_gmcp_suppress_minimap(ch)));

    sentience_send_package(d, "Sentience.Client.Preferences", obj);
}

/*
 * Safe string copy helper for cache fields.
 */
static void cache_strcpy(char *dst, const char *src, size_t sz)
{
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, sz - 1);
    dst[sz - 1] = '\0';
}

/* ----- Client.Layout storage helpers ----- */

web_client_layout_t *layout_find(web_client_layout_t *list, const char *name)
{
    for (web_client_layout_t *l = list; l; l = l->next)
        if (!str_cmp(l->name, name))
            return l;
    return NULL;
}

int layout_count(web_client_layout_t *list)
{
    int n = 0;
    for (web_client_layout_t *l = list; l; l = l->next)
        n++;
    return n;
}

void layout_free_all(web_client_layout_t **list)
{
    web_client_layout_t *l = *list, *next;
    while (l) {
        next = l->next;
        if (l->layout) json_decref(l->layout);
        free(l);
        l = next;
    }
    *list = NULL;
}

/* ── Client.Layout helpers ─────────────────────────────────────── */

static bool layout_name_is_valid(const char *name)
{
    int len;
    if (IS_NULLSTR(name)) return false;
    len = strlen(name);
    if (len < 1 || len > LAYOUT_NAME_MAX) return false;
    for (int i = 0; i < len; i++) {
        char c = name[i];
        if (!isalnum((unsigned char)c) && c != '_' && c != '-') return false;
    }
    return true;
}

static void sentience_send_layout_error(descriptor_t *d, const char *reason)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "action", json_string("error"));
    json_object_set_new(obj, "reason", json_string(reason));
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    sentience_send_package(d, "Sentience.Client.Layout", obj);
}

static void sentience_send_layout_saved(descriptor_t *d, const char *name)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "action", json_string("saved"));
    json_object_set_new(obj, "name", json_string(name));
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    sentience_send_package(d, "Sentience.Client.Layout", obj);
}

static void sentience_send_layout_deleted(descriptor_t *d, const char *name)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "action", json_string("deleted"));
    json_object_set_new(obj, "name", json_string(name));
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    sentience_send_package(d, "Sentience.Client.Layout", obj);
}

void sentience_send_layout_restore(descriptor_t *d, const char *name, json_t *layout)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "action", json_string("restore"));
    json_object_set_new(obj, "layout", json_incref(layout));
    json_object_set_new(obj, "name", json_string(name));
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    sentience_send_package(d, "Sentience.Client.Layout", obj);
}

static void sentience_send_layout_list(descriptor_t *d, CHAR_DATA *ch)
{
    json_t *obj = json_object();
    json_t *arr = json_array();
    web_client_layout_t *l;

    for (l = ch->pcdata->web_client_layouts; l; l = l->next)
        json_array_append_new(arr, json_string(l->name));

    json_object_set_new(obj, "action", json_string("list"));
    json_object_set_new(obj, "layouts", arr);
    json_object_set_new(obj, "active",
        ch->pcdata->active_layout[0] ? json_string(ch->pcdata->active_layout)
                                      : json_null());
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    sentience_send_package(d, "Sentience.Client.Layout", obj);
}

/*
 * sentience_handle_client_layout — process incoming Client.Layout messages.
 *
 * Actions: save, load, delete, list
 * Uses Jansson json_loads() to parse the full JSON body from ParseGMCP.
 */
void sentience_handle_client_layout(descriptor_t *d, const char *json_str)
{
    json_error_t err;
    json_t *root, *action_val, *name_val, *layout_val;
    const char *action, *name;
    CHAR_DATA *ch;

    if (!d || !(ch = d->character) || IS_NPC(ch) || !ch->pcdata)
        return;

    root = json_loads(json_str, 0, &err);
    if (!root || !json_is_object(root)) {
        sentience_send_layout_error(d, "invalid_payload");
        if (root) json_decref(root);
        return;
    }

    action_val = json_object_get(root, "action");
    if (!action_val || !json_is_string(action_val)) {
        sentience_send_layout_error(d, "invalid_action");
        json_decref(root);
        return;
    }
    action = json_string_value(action_val);

    if (!str_cmp(action, "save")) {
        name_val = json_object_get(root, "name");
        layout_val = json_object_get(root, "layout");

        if (!name_val || !json_is_string(name_val)) {
            sentience_send_layout_error(d, "invalid_name");
            json_decref(root);
            return;
        }
        name = json_string_value(name_val);

        if (!layout_val || !json_is_object(layout_val)) {
            sentience_send_layout_error(d, "invalid_payload");
            json_decref(root);
            return;
        }

        if (!layout_name_is_valid(name)) {
            sentience_send_layout_error(d, "invalid_name");
            json_decref(root);
            return;
        }

        /* Validate size */
        char *dump = json_dumps(layout_val, JSON_COMPACT);
        if (!dump) {
            sentience_send_layout_error(d, "invalid_payload");
            json_decref(root);
            return;
        }
        if (strlen(dump) > LAYOUT_MAX_SIZE) {
            free(dump);
            sentience_send_layout_error(d, "size_limit_exceeded");
            json_decref(root);
            return;
        }
        free(dump);

        /* Check if updating existing or adding new */
        web_client_layout_t *existing = layout_find(ch->pcdata->web_client_layouts, name);
        if (!existing && layout_count(ch->pcdata->web_client_layouts) >= LAYOUT_MAX_COUNT) {
            sentience_send_layout_error(d, "max_layouts_reached");
            json_decref(root);
            return;
        }

        if (existing) {
            if (existing->layout) json_decref(existing->layout);
            existing->layout = json_incref(layout_val);
        } else {
            web_client_layout_t *entry = calloc(1, sizeof(*entry));
            if (!entry) {
                json_decref(root);
                return;
            }
            snprintf(entry->name, sizeof(entry->name), "%s", name);
            entry->layout = json_incref(layout_val);
            entry->next = ch->pcdata->web_client_layouts;
            ch->pcdata->web_client_layouts = entry;
        }

        /* Set as active */
        snprintf(ch->pcdata->active_layout, sizeof(ch->pcdata->active_layout),
                 "%s", name);

        save_char_obj(ch);
        sentience_send_layout_saved(d, name);
    }
    else if (!str_cmp(action, "load")) {
        name_val = json_object_get(root, "name");
        if (!name_val || !json_is_string(name_val)) {
            sentience_send_layout_error(d, "invalid_name");
            json_decref(root);
            return;
        }
        name = json_string_value(name_val);

        if (!layout_name_is_valid(name)) {
            sentience_send_layout_error(d, "invalid_name");
            json_decref(root);
            return;
        }

        web_client_layout_t *entry = layout_find(ch->pcdata->web_client_layouts, name);
        if (!entry) {
            sentience_send_layout_error(d, "not_found");
            json_decref(root);
            return;
        }

        snprintf(ch->pcdata->active_layout, sizeof(ch->pcdata->active_layout),
                 "%s", name);
        save_char_obj(ch);
        sentience_send_layout_restore(d, entry->name, entry->layout);
    }
    else if (!str_cmp(action, "delete")) {
        name_val = json_object_get(root, "name");
        if (!name_val || !json_is_string(name_val)) {
            sentience_send_layout_error(d, "invalid_name");
            json_decref(root);
            return;
        }
        name = json_string_value(name_val);

        if (!layout_name_is_valid(name)) {
            sentience_send_layout_error(d, "invalid_name");
            json_decref(root);
            return;
        }

        web_client_layout_t *prev = NULL, *cur = ch->pcdata->web_client_layouts;
        while (cur) {
            if (!str_cmp(cur->name, name))
                break;
            prev = cur;
            cur = cur->next;
        }
        if (!cur) {
            sentience_send_layout_error(d, "not_found");
            json_decref(root);
            return;
        }

        if (prev) prev->next = cur->next;
        else ch->pcdata->web_client_layouts = cur->next;

        if (cur->layout) json_decref(cur->layout);
        free(cur);

        if (!str_cmp(ch->pcdata->active_layout, name))
            ch->pcdata->active_layout[0] = '\0';

        save_char_obj(ch);
        sentience_send_layout_deleted(d, name);
    }
    else if (!str_cmp(action, "list")) {
        sentience_send_layout_list(d, ch);
    }
    else {
        sentience_send_layout_error(d, "invalid_action");
    }

    json_decref(root);
}

/* ── Game-loop entry point ──────────────────────────────────────── */

void sentience_gmcp_update(descriptor_t *d)
{
    CHAR_DATA *ch;
    protocol_t *proto;
    sentience_gmcp_cache_t *cache;
    unsigned int dirty = 0;

    if (!d || !d->pProtocol)
        return;

    proto = d->pProtocol;

    if (!proto->bGMCP || !proto->bGMCPSupport[GMCP_SUPPORT_SENTIENCE])
        return;

    ch = d->character;
    if (!ch || !ch->in_room)
        return;

    cache = &proto->sentience_cache;

    /* ── Phase 3: Client.Ready.State (one-shot on first update) ─── */
    if (!cache->initialized) {
        sentience_send_package(d, "Sentience.Client.Ready.State",
            sentience_build_client_ready_state_json(PULSE_TICK, PULSE_PER_SECOND));
        sentience_send_client_preferences(d);
    }

    /* ── Detect changes ─────────────────────────────────────────── */

    /* On first send, mark everything dirty */
    if (!cache->initialized)
        dirty = 0xFF;

    /* Vitals */
    if (ch->hit != cache->hp || ch->max_hit != cache->max_hp
        || ch->mana != cache->mana || ch->max_mana != cache->max_mana
        || ch->move != cache->move || ch->max_move != cache->max_move) {
        dirty |= SENTIENCE_DIRTY_VITALS;
    }

    /* Stats */
    {
        int cur_str = get_curr_stat(ch, STAT_STR);
        int cur_int = get_curr_stat(ch, STAT_INT);
        int cur_wis = get_curr_stat(ch, STAT_WIS);
        int cur_dex = get_curr_stat(ch, STAT_DEX);
        int cur_con = get_curr_stat(ch, STAT_CON);

        if (cur_str != cache->str || cur_int != cache->int_
            || cur_wis != cache->wis || cur_dex != cache->dex
            || cur_con != cache->con
            || ch->perm_stat[STAT_STR] != cache->str_perm
            || ch->perm_stat[STAT_INT] != cache->int_perm
            || ch->perm_stat[STAT_WIS] != cache->wis_perm
            || ch->perm_stat[STAT_DEX] != cache->dex_perm
            || ch->perm_stat[STAT_CON] != cache->con_perm
            || GET_HITROLL(ch) != cache->hitroll
            || GET_DAMROLL(ch) != cache->damroll
            || ch->wimpy != cache->wimpy) {
            dirty |= SENTIENCE_DIRTY_STATS;
        }
    }

    /* Combat (AC) */
    if (GET_AC(ch, AC_PIERCE) != cache->ac_pierce
        || GET_AC(ch, AC_BASH) != cache->ac_bash
        || GET_AC(ch, AC_SLASH) != cache->ac_slash
        || GET_AC(ch, AC_EXOTIC) != cache->ac_exotic) {
        dirty |= SENTIENCE_DIRTY_COMBAT;
    }

    /* Worth */
    if (ch->alignment != cache->alignment
        || ch->exp != cache->xp
        || ch->practice != cache->practices
        || ch->gold != cache->gold) {
        dirty |= SENTIENCE_DIRTY_WORTH;
    }

    /* Identity — check level and name (sufficient for change detection) */
    if (ch->level != cache->level
        || ch->tot_level != cache->tot_level
        || strcmp(ch->name ? ch->name : "", cache->name) != 0) {
        dirty |= SENTIENCE_DIRTY_IDENTITY;
    }

    /* Room — check room vnum + area uid */
    {
        long rid0 = ch->in_room->area ? ch->in_room->area->uid : 0;
        long rid1 = ch->in_room->vnum;

        if (rid0 != cache->room_id0 || rid1 != cache->room_id1)
            dirty |= SENTIENCE_DIRTY_ROOM;
    }

    if (dirty == 0)
        return;

    /* ── Send dirty packages and update cache ───────────────────── */

    if (dirty & SENTIENCE_DIRTY_VITALS) {
        sentience_send_package(d, "Sentience.Char.Vitals",
            sentience_build_vitals_json(ch->hit, ch->max_hit,
                                         ch->mana, ch->max_mana,
                                         ch->move, ch->max_move));
        cache->hp       = ch->hit;
        cache->max_hp   = ch->max_hit;
        cache->mana     = ch->mana;
        cache->max_mana = ch->max_mana;
        cache->move     = ch->move;
        cache->max_move = ch->max_move;
    }

    if (dirty & SENTIENCE_DIRTY_STATS) {
        int cur_str = get_curr_stat(ch, STAT_STR);
        int cur_int = get_curr_stat(ch, STAT_INT);
        int cur_wis = get_curr_stat(ch, STAT_WIS);
        int cur_dex = get_curr_stat(ch, STAT_DEX);
        int cur_con = get_curr_stat(ch, STAT_CON);

        sentience_send_package(d, "Sentience.Char.Stats",
            sentience_build_stats_json(cur_str, cur_int, cur_wis, cur_dex, cur_con,
                                        ch->perm_stat[STAT_STR], ch->perm_stat[STAT_INT],
                                        ch->perm_stat[STAT_WIS], ch->perm_stat[STAT_DEX],
                                        ch->perm_stat[STAT_CON],
                                        GET_HITROLL(ch), GET_DAMROLL(ch), ch->wimpy));
        cache->str      = cur_str;
        cache->int_     = cur_int;
        cache->wis      = cur_wis;
        cache->dex      = cur_dex;
        cache->con      = cur_con;
        cache->str_perm = ch->perm_stat[STAT_STR];
        cache->int_perm = ch->perm_stat[STAT_INT];
        cache->wis_perm = ch->perm_stat[STAT_WIS];
        cache->dex_perm = ch->perm_stat[STAT_DEX];
        cache->con_perm = ch->perm_stat[STAT_CON];
        cache->hitroll  = GET_HITROLL(ch);
        cache->damroll  = GET_DAMROLL(ch);
        cache->wimpy    = ch->wimpy;
    }

    if (dirty & SENTIENCE_DIRTY_COMBAT) {
        sentience_send_package(d, "Sentience.Char.Combat",
            sentience_build_combat_json(GET_AC(ch, AC_PIERCE), GET_AC(ch, AC_BASH),
                                         GET_AC(ch, AC_SLASH), GET_AC(ch, AC_EXOTIC)));
        cache->ac_pierce = GET_AC(ch, AC_PIERCE);
        cache->ac_bash   = GET_AC(ch, AC_BASH);
        cache->ac_slash  = GET_AC(ch, AC_SLASH);
        cache->ac_exotic = GET_AC(ch, AC_EXOTIC);
    }

    if (dirty & SENTIENCE_DIRTY_WORTH) {
        long xp_tnl = ch->maxexp > 0 ? ch->maxexp : 0;

        sentience_send_package(d, "Sentience.Char.Worth",
            sentience_build_worth_json(ch->alignment, ch->exp, xp_tnl,
                                        ch->practice, ch->gold));
        cache->alignment = ch->alignment;
        cache->xp        = ch->exp;
        cache->xp_tnl    = xp_tnl;
        cache->practices  = ch->practice;
        cache->gold      = ch->gold;
    }

    if (dirty & SENTIENCE_DIRTY_IDENTITY) {
        sentience_identity_input_t data = {0};

        data.name      = ch->name ? ch->name : "";
        data.race_name = ch->race ? ch->race->name : "Unknown";
        data.race_wnum = "";
        data.body_type = get_body_type_name(ch);
        data.level     = ch->level;
        data.tot_level = ch->tot_level;
        data.title     = (ch->pcdata && ch->pcdata->title) ? ch->pcdata->title : "";

        /* Build class list */
        if (ch->pcdata && ch->pcdata->classes) {
            ITERATOR it;
            CLASS_LEVEL *cl;
            int i = 0;

            iterator_start(&it, ch->pcdata->classes);
            while ((cl = (CLASS_LEVEL *)iterator_nextdata(&it)) && i < SENTIENCE_MAX_CLASSES) {
                if (!cl->clazz)
                    continue;
                data.classes[i].name       = class_display_ch(cl->clazz, ch);
                data.classes[i].id         = cl->clazz->name ? cl->clazz->name : "";
                data.classes[i].level      = cl->level;
                data.classes[i].is_primary = (cl == ch->pcdata->current_class);
                i++;
            }
            iterator_stop(&it);
            data.num_classes = i;
        }

        sentience_send_package(d, "Sentience.Char.Identity",
            sentience_build_identity_json(&data));

        cache->level     = ch->level;
        cache->tot_level = ch->tot_level;
        cache_strcpy(cache->name, ch->name, sizeof(cache->name));
    }

    if (dirty & SENTIENCE_DIRTY_ROOM) {
        ROOM_INDEX_DATA *room = ch->in_room;
        sentience_room_input_t data = {0};
        int i;

        /* Copy widevnum strings to local buffers (rotating static buffers) */
        data.wnum      = widevnum_string_room(room, NULL);
        data.name      = room->name ? room->name : "";
        data.area_name = (room->area && room->area->name) ? room->area->name : "";
        data.area_wnum = room->area
            ? widevnum_string(room->area, 0, NULL) : "";
        data.sector    = (room->sector && room->sector->name)
            ? room->sector->name : "";

        if (ch->in_wilds) {
            data.is_wilds  = true;
            data.wilds_uid = ch->in_wilds->uid;
            data.wilds_x   = ch->at_wilds_x;
            data.wilds_y   = ch->at_wilds_y;
        }

        /* Build exits */
        for (i = 0; i < MAX_DIR && data.num_exits < 10; i++) {
            EXIT_DATA *ex = room->exit[i];
            if (!ex || !ex->u1.to_room)
                continue;
            data.exits[data.num_exits].dir       = dir_name[i];
            data.exits[data.num_exits].wnum      = widevnum_string_room(ex->u1.to_room, NULL);
            data.exits[data.num_exits].name      = ex->keyword ? ex->keyword : "";
            data.exits[data.num_exits].is_door   = IS_SET(ex->exit_info, EX_ISDOOR) ? true : false;
            data.exits[data.num_exits].is_closed = IS_SET(ex->exit_info, EX_CLOSED) ? true : false;
            data.exits[data.num_exits].is_locked = IS_SET(ex->exit_info, EX_LOCKED) ? true : false;
            data.num_exits++;
        }

        sentience_send_package(d, "Sentience.Room.Info",
            sentience_build_room_json(&data));

        /* Room.Map — send pre-rendered minimap */
        {
            BUFFER *map_buf = NULL;
            int map_w = 0, map_h = 0;
            bool has_map = false;

            if (ch->in_room->wilds) {
                int vp_x = get_squares_to_show_x(ch->wildview_bonus_x);
                int vp_y = get_squares_to_show_y(ch->wildview_bonus_y);
                has_map = render_wilds_map_to_buffer(ch->in_room->wilds,
                    ch->in_room->x, ch->in_room->y, ch, vp_x, vp_y,
                    &map_buf, &map_w, &map_h);
            } else {
                has_map = render_area_map_to_buffer(ch, ch->in_room,
                    &map_buf, &map_w, &map_h);
            }

            if (has_map && map_buf) {
                sentience_room_map_input_t map_input = {
                    .type     = ch->in_room->wilds ? "wilds" : "area",
                    .map_text = buf_string(map_buf),
                    .width    = map_w,
                    .height   = map_h,
                };
                sentience_send_package(d, "Sentience.Room.Map",
                    sentience_build_room_map(&map_input));
                free_buf(map_buf);
            }
        }

        cache->room_id0 = room->area ? room->area->uid : 0;
        cache->room_id1 = room->vnum;
    }

    /* ── Phase 3: Char.Affects ─────────────────────────────────── */
    if (ch->affected || cache->had_affects) {
        AFFECT_DATA *af;
        sentience_affect_input_t aff_inputs[64];
        int num_aff = 0;
        char mod_buf[64][64];

        for (af = ch->affected; af && num_aff < 64; af = af->next) {
            if (af->skill && af->skill->name)
                aff_inputs[num_aff].name = af->skill->name;
            else if (af->custom_name)
                aff_inputs[num_aff].name = af->custom_name;
            else
                aff_inputs[num_aff].name = "unknown";

            /* Skills don't have area/vnum like other entities, use NULL for wnum */
            aff_inputs[num_aff].wnum = NULL;

            aff_inputs[num_aff].duration = af->duration;
            aff_inputs[num_aff].estimated_seconds = (af->duration >= 0)
                ? (af->duration * PULSE_TICK / PULSE_PER_SECOND)
                : -1;

            if (af->location != APPLY_NONE && af->modifier != 0) {
                snprintf(mod_buf[num_aff], sizeof(mod_buf[num_aff]),
                         "%+d %s", af->modifier, affect_loc_name(af->location));
                aff_inputs[num_aff].modifier = mod_buf[num_aff];
            } else {
                aff_inputs[num_aff].modifier = NULL;
            }

            aff_inputs[num_aff].level = af->level;
            num_aff++;
        }

        sentience_send_package(d, "Sentience.Char.Affects",
            sentience_build_affects_json(aff_inputs, num_aff));
        cache->had_affects = (ch->affected != NULL);
    }

    /* ── Phase 3: Char.Enemies ─────────────────────────────────── */
    if (ch->fighting || cache->was_fighting) {
        sentience_enemy_input_t en_inputs[32];
        int num_en = 0;
        CHAR_DATA *vch;

        if (ch->fighting && ch->fighting->in_room == ch->in_room) {
            vch = ch->fighting;
            en_inputs[num_en].name = IS_NPC(vch) ? vch->short_descr : vch->name;
            en_inputs[num_en].instance_id[0] = vch->id[0];
            en_inputs[num_en].instance_id[1] = vch->id[1];
            en_inputs[num_en].hp_pct = (int)((vch->hit * 100) / UMAX(1, vch->max_hit));
            en_inputs[num_en].is_primary = true;
            en_inputs[num_en].target = (vch->fighting == ch) ? "you"
                : (vch->fighting ? (IS_NPC(vch->fighting) ? vch->fighting->short_descr
                                                           : vch->fighting->name)
                                 : "no one");
            num_en++;
        }

        for (vch = ch->in_room->people; vch && num_en < 32; vch = vch->next_in_room) {
            if (vch == ch || vch == ch->fighting || vch->fighting != ch)
                continue;
            en_inputs[num_en].name = IS_NPC(vch) ? vch->short_descr : vch->name;
            en_inputs[num_en].instance_id[0] = vch->id[0];
            en_inputs[num_en].instance_id[1] = vch->id[1];
            en_inputs[num_en].hp_pct = (int)((vch->hit * 100) / UMAX(1, vch->max_hit));
            en_inputs[num_en].is_primary = false;
            en_inputs[num_en].target = "you";
            num_en++;
        }

        sentience_send_package(d, "Sentience.Char.Enemies",
            sentience_build_enemies_json(en_inputs, num_en, ch->hit, ch->max_hit));
        cache->was_fighting = (ch->fighting != NULL);
    }

    /* ── Phase 3: Room.Contents (fingerprint comparison) ──────── */
    {
        long cur_rid[2];
        int cur_count = 0;

        cur_rid[0] = ch->in_room->area ? ch->in_room->area->uid : 0;
        cur_rid[1] = ch->in_room->vnum;

        {
            OBJ_DATA *obj;
            CHAR_DATA *rch;
            int dir;

            for (obj = ch->in_room->contents; obj; obj = obj->next_content) {
                if (can_see_obj(ch, obj))
                    cur_count++;
            }
            for (rch = ch->in_room->people; rch; rch = rch->next_in_room) {
                if (rch != ch && can_see(ch, rch))
                    cur_count++;
            }
            for (dir = 0; dir < MAX_DIR; dir++) {
                EXIT_DATA *ex = ch->in_room->exit[dir];
                if (ex && ex->u1.to_room && IS_SET(ex->exit_info, EX_ISDOOR))
                    cur_count++;
            }
        }

        if (cur_rid[0] != cache->contents_room_id[0]
            || cur_rid[1] != cache->contents_room_id[1]
            || cur_count != cache->contents_count) {

            sentience_room_entity_input_t item_inputs[128];
            sentience_room_entity_input_t npc_inputs[64];
            sentience_room_entity_input_t player_inputs[64];
            sentience_room_door_input_t door_inputs[10];
            sentience_room_contents_input_t data = {0};
            OBJ_DATA *obj;
            CHAR_DATA *rch;
            int dir;

            for (obj = ch->in_room->contents; obj; obj = obj->next_content) {
                if (!can_see_obj(ch, obj) || data.num_items >= 128)
                    continue;
                item_inputs[data.num_items].name = obj->short_descr ? obj->short_descr : "something";
                item_inputs[data.num_items].instance_id[0] = obj->id[0];
                item_inputs[data.num_items].instance_id[1] = obj->id[1];
                item_inputs[data.num_items].short_desc = obj->description ? obj->description : "";
                data.num_items++;
            }
            data.items = item_inputs;

            for (rch = ch->in_room->people; rch; rch = rch->next_in_room) {
                if (rch == ch || !can_see(ch, rch))
                    continue;

                if (IS_NPC(rch)) {
                    if (data.num_npcs >= 64) continue;
                    npc_inputs[data.num_npcs].name = rch->short_descr ? rch->short_descr : "someone";
                    npc_inputs[data.num_npcs].instance_id[0] = rch->id[0];
                    npc_inputs[data.num_npcs].instance_id[1] = rch->id[1];
                    npc_inputs[data.num_npcs].short_desc = rch->long_descr ? rch->long_descr : "";
                    data.num_npcs++;
                } else {
                    if (data.num_players >= 64) continue;
                    player_inputs[data.num_players].name = rch->name ? rch->name : "someone";
                    player_inputs[data.num_players].instance_id[0] = 0;
                    player_inputs[data.num_players].instance_id[1] = 0;
                    player_inputs[data.num_players].short_desc = NULL;
                    data.num_players++;
                }
            }
            data.npcs = npc_inputs;
            data.players = player_inputs;

            for (dir = 0; dir < MAX_DIR && data.num_doors < 10; dir++) {
                EXIT_DATA *ex = ch->in_room->exit[dir];
                if (!ex || !ex->u1.to_room || !IS_SET(ex->exit_info, EX_ISDOOR))
                    continue;
                door_inputs[data.num_doors].direction = dir_name[dir];
                if (IS_SET(ex->exit_info, EX_LOCKED))
                    door_inputs[data.num_doors].state = "locked";
                else if (IS_SET(ex->exit_info, EX_CLOSED))
                    door_inputs[data.num_doors].state = "closed";
                else
                    door_inputs[data.num_doors].state = "open";
                door_inputs[data.num_doors].is_locked = IS_SET(ex->exit_info, EX_LOCKED) ? true : false;
                data.num_doors++;
            }
            data.doors = door_inputs;

            sentience_send_package(d, "Sentience.Room.Contents",
                sentience_build_room_contents_json(&data));

            cache->contents_room_id[0] = cur_rid[0];
            cache->contents_room_id[1] = cur_rid[1];
            cache->contents_count = cur_count;
        }
    }

    cache->initialized = true;
}

/*
 * Reset the cache to force a full resend on next update cycle.
 */
void sentience_gmcp_cache_reset(sentience_gmcp_cache_t *cache)
{
    if (!cache)
        return;
    memset(cache, 0, sizeof(*cache));
}

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
    int i, j;

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
        const sentience_identity_class_t *c = &data->classes[i];
        json_t *cls = json_object();
        
        /* Original fields (preserve backward compatibility) */
        json_object_set_new(cls, "id",         json_string(c->id ? c->id : ""));
        json_object_set_new(cls, "name",       json_string(c->name ? c->name : ""));
        json_object_set_new(cls, "level",      json_integer(c->level));
        json_object_set_new(cls, "is_primary", c->is_primary ? json_true() : json_false());
        
        /* Extended fields */
        json_object_set_new(cls, "max_level",    json_integer(c->max_level));
        json_object_set_new(cls, "type",         json_string(c->type ? c->type : ""));
        json_object_set_new(cls, "flags",        json_string(c->flags ? c->flags : ""));
        json_object_set_new(cls, "primary_stat", json_string(c->primary_stat ? c->primary_stat : ""));

        json_t *hp_range = json_array();
        json_array_append_new(hp_range, json_integer(c->hp_min));
        json_array_append_new(hp_range, json_integer(c->hp_max));
        json_object_set_new(cls, "hp_range", hp_range);

        json_object_set_new(cls, "gains_mana",   c->gains_mana ? json_true() : json_false());
        json_object_set_new(cls, "description",  json_string(c->description ? c->description : ""));
        json_object_set_new(cls, "xp",           json_integer(c->xp));
        json_object_set_new(cls, "active_title", c->active_title ? json_string(c->active_title) : json_null());

        /* available_titles */
        json_t *titles_arr = json_array();
        for (j = 0; j < c->num_titles; j++) {
            json_t *t = json_object();
            json_object_set_new(t, "keyword", json_string(c->titles[j].keyword ? c->titles[j].keyword : ""));
            json_object_set_new(t, "display", json_string(c->titles[j].display ? c->titles[j].display : ""));
            json_object_set_new(t, "is_default", c->titles[j].is_default ? json_true() : json_false());
            json_array_append_new(titles_arr, t);
        }
        json_object_set_new(cls, "available_titles", titles_arr);

        /* action — null for primary, object for secondary */
        if (c->action_label && c->action_cmd) {
            json_t *action = json_object();
            json_object_set_new(action, "label", json_string(c->action_label));
            json_object_set_new(action, "cmd", json_string(c->action_cmd));
            json_object_set_new(cls, "action", action);
        } else {
            json_object_set_new(cls, "action", json_null());
        }
        
        json_array_append_new(classes, cls);
    }
    json_object_set_new(obj, "classes", classes);

    /* traits array */
    json_t *traits_arr = json_array();
    for (i = 0; i < data->num_traits; i++) {
        const sentience_trait_t *tr = &data->traits[i];
        json_t *jtrait = json_object();
        json_object_set_new(jtrait, "id", json_string(tr->id ? tr->id : ""));
        json_object_set_new(jtrait, "name", json_string(tr->name ? tr->name : ""));
        json_object_set_new(jtrait, "description", json_string(tr->description ? tr->description : ""));
        json_object_set_new(jtrait, "category", json_string(tr->category ? tr->category : ""));
        json_object_set_new(jtrait, "type", json_string(tr->type ? tr->type : "bool"));
        json_object_set_new(jtrait, "source", json_string(tr->source ? tr->source : ""));
        /* value: typed based on tr->type */
        if (tr->type && !strcmp(tr->type, "int"))
            json_object_set_new(jtrait, "value", json_integer(tr->value_int));
        else if (tr->type && !strcmp(tr->type, "string"))
            json_object_set_new(jtrait, "value", json_string(tr->value_string ? tr->value_string : ""));
        else
            json_object_set_new(jtrait, "value", tr->value_bool ? json_true() : json_false());
        json_array_append_new(traits_arr, jtrait);
    }
    json_object_set_new(obj, "traits", traits_arr);

    /* race_info object */
    json_t *race_info = json_object();
    const sentience_race_info_t *ri = &data->race_info;
    json_object_set_new(race_info, "id", json_string(ri->id ? ri->id : ""));
    json_object_set_new(race_info, "name", json_string(ri->name ? ri->name : ""));
    json_object_set_new(race_info, "description", json_string(ri->description ? ri->description : ""));
    json_object_set_new(race_info, "playable", ri->playable ? json_true() : json_false());
    json_object_set_new(race_info, "starting", ri->starting ? json_true() : json_false());
    json_object_set_new(race_info, "size", json_string(ri->size ? ri->size : "medium"));

    /* stats object */
    json_t *stats = json_object();
    json_object_set_new(stats, "str", json_integer(ri->stats[0]));
    json_object_set_new(stats, "int", json_integer(ri->stats[1]));
    json_object_set_new(stats, "wis", json_integer(ri->stats[2]));
    json_object_set_new(stats, "dex", json_integer(ri->stats[3]));
    json_object_set_new(stats, "con", json_integer(ri->stats[4]));
    json_object_set_new(race_info, "stats", stats);

    /* max_stats object */
    json_t *max_stats = json_object();
    json_object_set_new(max_stats, "str", json_integer(ri->max_stats[0]));
    json_object_set_new(max_stats, "int", json_integer(ri->max_stats[1]));
    json_object_set_new(max_stats, "wis", json_integer(ri->max_stats[2]));
    json_object_set_new(max_stats, "dex", json_integer(ri->max_stats[3]));
    json_object_set_new(max_stats, "con", json_integer(ri->max_stats[4]));
    json_object_set_new(race_info, "max_stats", max_stats);

    /* max_vitals object */
    json_t *max_vitals = json_object();
    json_object_set_new(max_vitals, "hp", json_integer(ri->max_vitals[0]));
    json_object_set_new(max_vitals, "mana", json_integer(ri->max_vitals[1]));
    json_object_set_new(max_vitals, "move", json_integer(ri->max_vitals[2]));
    json_object_set_new(race_info, "max_vitals", max_vitals);

    /* skills array */
    json_t *skills_arr = json_array();
    for (i = 0; i < ri->num_skills; i++)
        json_array_append_new(skills_arr, json_string(ri->skills[i] ? ri->skills[i] : ""));
    json_object_set_new(race_info, "skills", skills_arr);

    json_object_set_new(race_info, "resistances", json_string(ri->resistances ? ri->resistances : ""));
    json_object_set_new(race_info, "vulnerabilities", json_string(ri->vulnerabilities ? ri->vulnerabilities : ""));
    json_object_set_new(race_info, "immunities", json_string(ri->immunities ? ri->immunities : ""));
    json_object_set_new(race_info, "affects", json_string(ri->affects ? ri->affects : ""));
    json_object_set_new(race_info, "remort_into", ri->remort_into ? json_string(ri->remort_into) : json_null());

    /* race traits */
    json_t *race_traits_arr = json_array();
    for (i = 0; i < ri->num_traits; i++) {
        const sentience_trait_t *rt = &ri->traits[i];
        json_t *rtrait = json_object();
        json_object_set_new(rtrait, "id", json_string(rt->id ? rt->id : ""));
        json_object_set_new(rtrait, "name", json_string(rt->name ? rt->name : ""));
        json_object_set_new(rtrait, "type", json_string(rt->type ? rt->type : "bool"));
        if (rt->type && !strcmp(rt->type, "int"))
            json_object_set_new(rtrait, "value", json_integer(rt->value_int));
        else if (rt->type && !strcmp(rt->type, "string"))
            json_object_set_new(rtrait, "value", json_string(rt->value_string ? rt->value_string : ""));
        else
            json_object_set_new(rtrait, "value", rt->value_bool ? json_true() : json_false());
        json_array_append_new(race_traits_arr, rtrait);
    }
    json_object_set_new(race_info, "traits", race_traits_arr);

    json_object_set_new(obj, "race_info", race_info);
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
    int i;

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

    if (input->report_id)
        json_object_set_new(obj, "report_id", json_string(input->report_id));

    if (input->num_actions > 0) {
        json_t *actions = json_array();
        for (i = 0; i < input->num_actions; i++) {
            json_t *act = json_object();
            json_object_set_new(act, "label", json_string(input->actions[i].label ? input->actions[i].label : ""));
            json_object_set_new(act, "cmd",   json_string(input->actions[i].cmd ? input->actions[i].cmd : ""));
            json_array_append_new(actions, act);
        }
        json_object_set_new(obj, "actions", actions);
    }

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
    json_array_append_new(packages, json_string("Sentience.Client.Layout"));
    json_array_append_new(packages, json_string("Sentience.Auth.QRCode"));
    json_array_append_new(packages, json_string("Sentience.Link"));
    json_array_append_new(packages, json_string("Sentience.Char.Inventory"));
    json_array_append_new(packages, json_string("Sentience.Char.Equipment"));
    json_array_append_new(packages, json_string("Sentience.Char.Abilities"));
    json_array_append_new(packages, json_string("Sentience.Char.Reputations"));
    json_array_append_new(packages, json_string("Sentience.Char.Church"));
    json_array_append_new(packages, json_string("Sentience.Char.Race"));

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

/* ── Auth.QRCode builder & sender ──────────────────────────────── */

json_t *sentience_build_auth_qrcode_json(const char *purpose, const char *image,
                                          const char *uri, long expires_at)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "purpose", json_string(purpose ? purpose : "totp_setup"));
    json_object_set_new(obj, "image", json_string(image ? image : ""));
    json_object_set_new(obj, "uri", json_string(uri ? uri : ""));
    json_object_set_new(obj, "expires_at", json_integer(expires_at));

    return obj;
}

void sentience_send_auth_qrcode(descriptor_t *d, const char *image_data_url,
                                 const char *uri, long expires_at)
{
    json_t *obj = sentience_build_auth_qrcode_json("totp_setup", image_data_url,
                                                    uri, expires_at);
    if (obj)
        sentience_send_package(d, "Sentience.Auth.QRCode", obj);
}

json_t *sentience_build_preferences_json(const sentience_preferences_input_t *input)
{
    json_t *obj, *prefs;
    int i;

    if (!input) return NULL;

    obj = json_object();
    if (!obj) return NULL;
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    prefs = json_array();
    for (i = 0; i < input->num_prefs; i++) {
        const sentience_pref_entry_t *p = &input->prefs[i];
        json_t *entry = json_object();

        json_object_set_new(entry, "key",      json_string(p->key ? p->key : ""));
        json_object_set_new(entry, "category", json_string(p->category ? p->category : ""));
        json_object_set_new(entry, "type",     json_string(p->type ? p->type : "bool"));
        json_object_set_new(entry, "source",   json_string(p->source ? p->source : "default"));
        json_object_set_new(entry, "label",    json_string(p->label ? p->label : ""));

        if (p->type && !strcmp(p->type, "int"))
            json_object_set_new(entry, "value", json_integer(p->value_int));
        else if (p->type && !strcmp(p->type, "string"))
            json_object_set_new(entry, "value", json_string(p->value_string ? p->value_string : ""));
        else
            json_object_set_new(entry, "value", p->value_bool ? json_true() : json_false());

        json_array_append_new(prefs, entry);
    }
    json_object_set_new(obj, "preferences", prefs);

    return obj;
}

json_t *sentience_build_inventory_json(const sentience_inventory_input_t *input)
{
    json_t *obj, *items_arr, *capacity;
    int i;

    if (!input) return NULL;

    obj = json_object();
    if (!obj) return NULL;
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    items_arr = json_array();
    for (i = 0; i < input->num_items; i++) {
        const sentience_inventory_item_t *item = &input->items[i];
        json_t *jitem = json_object();
        json_t *id_arr, *flags_arr, *actions_arr;
        int j;

        /* ID as 2-element array */
        id_arr = json_array();
        json_array_append_new(id_arr, json_integer(item->id[0]));
        json_array_append_new(id_arr, json_integer(item->id[1]));
        json_object_set_new(jitem, "id", id_arr);

        json_object_set_new(jitem, "name", json_string(item->name ? item->name : ""));
        json_object_set_new(jitem, "keywords", json_string(item->keywords ? item->keywords : ""));
        json_object_set_new(jitem, "keyword", json_string(item->keyword ? item->keyword : ""));
        json_object_set_new(jitem, "item_type", json_string(item->item_type ? item->item_type : ""));
        json_object_set_new(jitem, "level", json_integer(item->level));
        json_object_set_new(jitem, "weight", json_integer(item->weight));
        json_object_set_new(jitem, "condition", json_integer(item->condition));
        json_object_set_new(jitem, "condition_label", json_string(item->condition_label ? item->condition_label : ""));
        json_object_set_new(jitem, "item_count", json_integer(item->item_count));

        /* Flags array */
        flags_arr = json_array();
        for (j = 0; j < item->num_flags; j++)
            json_array_append_new(flags_arr, json_string(item->flags[j] ? item->flags[j] : ""));
        json_object_set_new(jitem, "flags", flags_arr);

        /* Actions array */
        actions_arr = json_array();
        for (j = 0; j < item->num_actions; j++) {
            json_t *action = json_object();
            json_object_set_new(action, "label", json_string(item->actions[j].label ? item->actions[j].label : ""));
            json_object_set_new(action, "cmd", json_string(item->actions[j].cmd ? item->actions[j].cmd : ""));
            json_array_append_new(actions_arr, action);
        }
        json_object_set_new(jitem, "actions", actions_arr);

        json_array_append_new(items_arr, jitem);
    }
    json_object_set_new(obj, "items", items_arr);

    /* Capacity object */
    capacity = json_object();
    json_object_set_new(capacity, "items", json_integer(input->capacity_current_items));
    json_object_set_new(capacity, "max_items", json_integer(input->capacity_max_items));
    json_object_set_new(capacity, "weight", json_integer(input->capacity_current_weight));
    json_object_set_new(capacity, "max_weight", json_integer(input->capacity_max_weight));
    json_object_set_new(capacity, "coin_weight", json_integer(input->capacity_coin_weight));
    json_object_set_new(obj, "capacity", capacity);

    return obj;
}

json_t *sentience_build_equipment_json(const sentience_equipment_input_t *input)
{
    json_t *obj, *slots_arr;
    int i;

    if (!input) return NULL;

    obj = json_object();
    if (!obj) return NULL;
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    slots_arr = json_array();
    for (i = 0; i < input->num_slots; i++) {
        const sentience_equipment_slot_t *slot = &input->slots[i];
        json_t *jslot = json_object();

        json_object_set_new(jslot, "slot_id", json_integer(slot->slot_id));
        json_object_set_new(jslot, "slot_name", json_string(slot->slot_name ? slot->slot_name : ""));

        if (slot->occupied) {
            json_t *jitem = json_object();
            json_t *id_arr, *flags_arr, *actions_arr;
            int j;

            id_arr = json_array();
            json_array_append_new(id_arr, json_integer(slot->id[0]));
            json_array_append_new(id_arr, json_integer(slot->id[1]));
            json_object_set_new(jitem, "id", id_arr);

            json_object_set_new(jitem, "name", json_string(slot->item_name ? slot->item_name : ""));
            json_object_set_new(jitem, "keywords", json_string(slot->keywords ? slot->keywords : ""));
            json_object_set_new(jitem, "item_type", json_string(slot->item_type ? slot->item_type : ""));
            json_object_set_new(jitem, "level", json_integer(slot->level));
            json_object_set_new(jitem, "condition", json_integer(slot->condition));
            json_object_set_new(jitem, "condition_label", json_string(slot->condition_label ? slot->condition_label : ""));

            flags_arr = json_array();
            for (j = 0; j < slot->num_flags; j++)
                json_array_append_new(flags_arr, json_string(slot->flags[j] ? slot->flags[j] : ""));
            json_object_set_new(jitem, "flags", flags_arr);

            actions_arr = json_array();
            for (j = 0; j < slot->num_actions; j++) {
                json_t *action = json_object();
                json_object_set_new(action, "label", json_string(slot->actions[j].label ? slot->actions[j].label : ""));
                json_object_set_new(action, "cmd", json_string(slot->actions[j].cmd ? slot->actions[j].cmd : ""));
                json_array_append_new(actions_arr, action);
            }
            json_object_set_new(jitem, "actions", actions_arr);

            json_object_set_new(jslot, "item", jitem);
        } else {
            json_object_set_new(jslot, "item", json_null());
        }

        json_array_append_new(slots_arr, jslot);
    }
    json_object_set_new(obj, "slots", slots_arr);

    return obj;
}

json_t *sentience_build_abilities_json(const sentience_abilities_input_t *input)
{
    json_t *obj;
    json_t *abilities;
    int i, j;

    if (!input) return NULL;

    obj = json_object();
    if (!obj) return NULL;

    abilities = json_array();
    for (i = 0; i < input->num_abilities; i++) {
        const sentience_ability_t *a = &input->abilities[i];
        json_t *ability = json_object();

        json_object_set_new(ability, "name",         json_string(a->name ? a->name : ""));
        json_object_set_new(ability, "type",         json_string(a->type ? a->type : "skill"));
        json_object_set_new(ability, "available",    a->available ? json_true() : json_false());
        json_object_set_new(ability, "rating",       json_integer(a->rating));
        json_object_set_new(ability, "modifier",     json_integer(a->modifier));
        json_object_set_new(ability, "mana",         json_integer(a->mana));
        json_object_set_new(ability, "level",        json_integer(a->level));
        json_object_set_new(ability, "target",       json_string(a->target ? a->target : ""));
        json_object_set_new(ability, "can_practice", a->can_practice ? json_true() : json_false());
        json_object_set_new(ability, "learn_rate",   json_integer(a->learn_rate));

        json_t *actions = json_array();
        for (j = 0; j < a->num_actions; j++) {
            json_t *act = json_object();
            json_object_set_new(act, "label", json_string(a->actions[j].label ? a->actions[j].label : ""));
            json_object_set_new(act, "cmd",   json_string(a->actions[j].cmd ? a->actions[j].cmd : ""));
            json_array_append_new(actions, act);
        }
        json_object_set_new(ability, "actions", actions);

        json_array_append_new(abilities, ability);
    }

    json_object_set_new(obj, "abilities", abilities);
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_reputations_json(const sentience_reputations_input_t *input)
{
    json_t *obj;
    json_t *reps;
    int i;

    if (!input) return NULL;

    obj = json_object();
    if (!obj) return NULL;

    reps = json_array();
    for (i = 0; i < input->num_reputations; i++) {
        const sentience_reputation_t *r = &input->reputations[i];
        json_t *rep = json_object();

        json_object_set_new(rep, "name",          json_string(r->name ? r->name : ""));
        json_object_set_new(rep, "rank",          json_string(r->rank ? r->rank : ""));
        json_object_set_new(rep, "rank_color",    json_string(r->rank_color ? r->rank_color : ""));
        json_object_set_new(rep, "points",        json_integer(r->points));
        json_object_set_new(rep, "paragon_level", json_integer(r->paragon_level));
        json_object_set_new(rep, "max_rank",      json_string(r->max_rank ? r->max_rank : ""));

        json_array_append_new(reps, rep);
    }

    json_object_set_new(obj, "reputations", reps);
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_church_json(const sentience_church_input_t *input)
{
    json_t *obj;
    json_t *actions;
    int i;

    if (!input) return NULL;

    obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "is_member", input->is_member ? json_true() : json_false());

    if (input->is_member) {
        json_object_set_new(obj, "church_name", json_string(input->church_name ? input->church_name : ""));
        json_object_set_new(obj, "church_flag", json_string(input->church_flag ? input->church_flag : ""));
        json_object_set_new(obj, "alignment",   json_string(input->alignment ? input->alignment : ""));
        json_object_set_new(obj, "size",        json_string(input->size ? input->size : ""));
        json_object_set_new(obj, "pk",          input->pk ? json_true() : json_false());
        json_object_set_new(obj, "rank_name",   json_string(input->rank_name ? input->rank_name : ""));
        json_object_set_new(obj, "rank_type",   json_string(input->rank_type ? input->rank_type : ""));
        json_object_set_new(obj, "rank_title",  json_string(input->rank_title ? input->rank_title : ""));

        actions = json_array();
        for (i = 0; i < input->num_actions; i++) {
            json_t *act = json_object();
            json_object_set_new(act, "label", json_string(input->actions[i].label ? input->actions[i].label : ""));
            json_object_set_new(act, "cmd",   json_string(input->actions[i].cmd ? input->actions[i].cmd : ""));
            json_array_append_new(actions, act);
        }
        json_object_set_new(obj, "actions", actions);
    }

    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

static json_t *split_to_json_array(const char *space_separated)
{
    json_t *arr;
    const char *p;
    const char *start;

    arr = json_array();
    if (!space_separated || !*space_separated)
        return arr;

    p = space_separated;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        start = p;
        while (*p && *p != ' ') p++;
        {
            char buf[256];
            int len = (int)(p - start);
            if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
            memcpy(buf, start, len);
            buf[len] = '\0';
            json_array_append_new(arr, json_string(buf));
        }
    }

    return arr;
}

json_t *sentience_build_race_json(const sentience_race_info_t *input)
{
    json_t *obj;
    json_t *stats;
    json_t *max_stats;
    json_t *max_vitals;
    json_t *skills;
    json_t *traits_arr;
    int i;

    if (!input) return NULL;

    obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "id",          json_string(input->id ? input->id : ""));
    json_object_set_new(obj, "name",        json_string(input->name ? input->name : ""));
    json_object_set_new(obj, "description", json_string(input->description ? input->description : ""));
    json_object_set_new(obj, "playable",    input->playable ? json_true() : json_false());
    json_object_set_new(obj, "starting",    input->starting ? json_true() : json_false());
    json_object_set_new(obj, "size",        json_string(input->size ? input->size : "medium"));

    /* stats object */
    stats = json_object();
    json_object_set_new(stats, "str", json_integer(input->stats[0]));
    json_object_set_new(stats, "int", json_integer(input->stats[1]));
    json_object_set_new(stats, "wis", json_integer(input->stats[2]));
    json_object_set_new(stats, "dex", json_integer(input->stats[3]));
    json_object_set_new(stats, "con", json_integer(input->stats[4]));
    json_object_set_new(obj, "stats", stats);

    /* max_stats object */
    max_stats = json_object();
    json_object_set_new(max_stats, "str", json_integer(input->max_stats[0]));
    json_object_set_new(max_stats, "int", json_integer(input->max_stats[1]));
    json_object_set_new(max_stats, "wis", json_integer(input->max_stats[2]));
    json_object_set_new(max_stats, "dex", json_integer(input->max_stats[3]));
    json_object_set_new(max_stats, "con", json_integer(input->max_stats[4]));
    json_object_set_new(obj, "max_stats", max_stats);

    /* max_vitals object */
    max_vitals = json_object();
    json_object_set_new(max_vitals, "hp",   json_integer(input->max_vitals[0]));
    json_object_set_new(max_vitals, "mana", json_integer(input->max_vitals[1]));
    json_object_set_new(max_vitals, "move", json_integer(input->max_vitals[2]));
    json_object_set_new(obj, "max_vitals", max_vitals);

    /* skills array */
    skills = json_array();
    for (i = 0; i < input->num_skills; i++)
        json_array_append_new(skills, json_string(input->skills[i] ? input->skills[i] : ""));
    json_object_set_new(obj, "skills", skills);

    /* Split space-separated flags into arrays */
    json_object_set_new(obj, "resistances",     split_to_json_array(input->resistances));
    json_object_set_new(obj, "vulnerabilities",  split_to_json_array(input->vulnerabilities));
    json_object_set_new(obj, "immunities",       split_to_json_array(input->immunities));
    json_object_set_new(obj, "affects",          split_to_json_array(input->affects));

    json_object_set_new(obj, "remort_into", input->remort_into ? json_string(input->remort_into) : json_null());

    /* traits array */
    traits_arr = json_array();
    for (i = 0; i < input->num_traits; i++) {
        const sentience_trait_t *t = &input->traits[i];
        json_t *trait = json_object();
        json_object_set_new(trait, "id",   json_string(t->id ? t->id : ""));
        json_object_set_new(trait, "name", json_string(t->name ? t->name : ""));
        json_object_set_new(trait, "type", json_string(t->type ? t->type : "bool"));
        if (t->type && !strcmp(t->type, "int"))
            json_object_set_new(trait, "value", json_integer(t->value_int));
        else if (t->type && !strcmp(t->type, "string"))
            json_object_set_new(trait, "value", json_string(t->value_string ? t->value_string : ""));
        else
            json_object_set_new(trait, "value", t->value_bool ? json_true() : json_false());
        json_array_append_new(traits_arr, trait);
    }
    json_object_set_new(obj, "traits", traits_arr);

    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

/**
 * sentience_send_client_preferences - Send current GMCP prefs to client
 *
 * Sends Sentience.Client.Preferences with the current effective values.
 * Called on login and after a client preference update.
 */
void sentience_send_client_preferences(descriptor_t *d)
{
    CHAR_DATA *ch;
    ACCOUNT_DATA *account = NULL;
    bool account_loaded = false;
    sentience_preferences_input_t input = {0};
    PREF_ENTRY *pref;
    json_t *obj;
    int i;

    if (!d || !d->character)
        return;

    ch = d->character;
    if (ch->pcdata && ch->pcdata->account_name[0]) {
        account = get_account_online_or_offline(ch->pcdata->account_name, &account_loaded);
    }

    /* Walk pc_set_table[] for toggle preferences */
    for (i = 0; pc_set_table[i].name && input.num_prefs < SENTIENCE_MAX_PREFERENCES; i++) {
        sentience_pref_entry_t *entry = &input.prefs[input.num_prefs];

        entry->key = pc_set_table[i].name;
        entry->category = "toggle";
        entry->type = "bool";
        entry->label = pc_set_table[i].name;

        entry->source = pref_source_name(pref_get_source(account, ch, pc_set_table[i].name));
        entry->value_bool = pref_get_bool(account, ch, pc_set_table[i].name, 
                                         pc_set_table[i].default_state == SETTING_ON);

        input.num_prefs++;
    }

    /* Walk game_settings.pref_defaults for non-toggle preferences */
    for (pref = game_settings.pref_defaults; pref && input.num_prefs < SENTIENCE_MAX_PREFERENCES; pref = pref->next) {
        sentience_pref_entry_t *entry;

        /* Skip toggles already handled above */
        if (pref->category == PREF_CAT_TOGGLE)
            continue;

        entry = &input.prefs[input.num_prefs];

        entry->key = pref->key;
        entry->category = pref_category_name(pref->category);
        entry->type = pref_type_name(pref->type);
        entry->label = pref->key;
        entry->source = pref_source_name(pref_get_source(account, ch, pref->key));

        switch (pref->type) {
            case PREF_TYPE_BOOL:
                entry->value_bool = pref_get_bool(account, ch, pref->key, pref->val.b);
                break;
            case PREF_TYPE_INT:
                entry->value_int = pref_get_int(account, ch, pref->key, pref->val.i);
                break;
            case PREF_TYPE_STRING:
                entry->value_string = pref_get_string(account, ch, pref->key, pref->val.str);
                break;
            case PREF_TYPE_BITFIELD:
                entry->value_int = (int) pref_get_bitfield(account, ch, pref->key, pref->val.bits);
                break;
            default:
                break;
        }

        input.num_prefs++;
    }

    /* Build and send JSON */
    obj = sentience_build_preferences_json(&input);
    if (obj) {
        sentience_send_package(d, "Sentience.Client.Preferences", obj);
    }

    /* Cleanup account if we loaded it */
    if (account_loaded && account) {
        free_account(account);
    }
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

#ifdef BUILD_TESTS
bool test_layout_name_is_valid(const char *name)
{
    return layout_name_is_valid(name);
}
#endif

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
    if (!ch || !ch->in_room || d->connected != CON_PLAYING)
        return;

    cache = &proto->sentience_cache;

    /* ── Phase 3: Client.Ready.State (one-shot on first update) ─── */
    if (!cache->initialized) {
        sentience_send_package(d, "Sentience.Client.Ready.State",
            sentience_build_client_ready_state_json(PULSE_TICK, PULSE_PER_SECOND));
        sentience_send_client_preferences(d);

        /* Restore active layout for WebSocket clients */
        if (ch->pcdata && ch->pcdata->active_layout[0]
            && d->conn && d->conn->type == CONN_TYPE_WEBSOCKET_TLS) {
            web_client_layout_t *active = layout_find(
                ch->pcdata->web_client_layouts, ch->pcdata->active_layout);
            if (active && active->layout)
                sentience_send_layout_restore(d, active->name, active->layout);
        }
    }

    /* ── Detect changes ─────────────────────────────────────────── */

    /* On first send, mark everything dirty */
    if (!cache->initialized)
        dirty = SENTIENCE_DIRTY_ALL;

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

    /* Inventory — count visible carried items */
    {
        int cur_count = 0;
        ITERATOR it;
        OBJ_DATA *obj;

        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (can_see_obj(ch, obj))
                cur_count++;
        }
        iterator_stop(&it);

        if (cur_count != cache->inventory_count)
            dirty |= SENTIENCE_DIRTY_INVENTORY;
    }

    /* Equipment — count worn items */
    {
        int cur_count = 0;
        ITERATOR it;
        OBJ_DATA *obj;

        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            cur_count++;
        }
        iterator_stop(&it);

        if (cur_count != cache->equipment_count)
            dirty |= SENTIENCE_DIRTY_EQUIPMENT;
    }

    /* Abilities — count skills + songs (cached; -1 forces rebuild) */
    {
        int cur_count = 0;
        SKILL_ENTRY *entry;

        for (entry = ch->sorted_skills; entry; entry = entry->next)
            cur_count++;
        for (entry = ch->sorted_songs; entry; entry = entry->next)
            cur_count++;

        if (cur_count != cache->abilities_count)
            dirty |= SENTIENCE_DIRTY_ABILITIES;
    }

    /* Reputations — count factions (-1 forces rebuild) */
    {
        int cur_count = 0;

        if (ch->reputations) {
            ITERATOR it;
            REPUTATION_DATA *rep;

            iterator_start(&it, ch->reputations);
            while ((rep = (REPUTATION_DATA *)iterator_nextdata(&it)) != NULL) {
                if (rep->pIndexData && !IS_SET(rep->pIndexData->flags, REPUTATION_HIDDEN))
                    cur_count++;
            }
            iterator_stop(&it);
        }

        if (cur_count != cache->reputation_count)
            dirty |= SENTIENCE_DIRTY_REPUTATIONS;
    }

    /* Church — check membership and church uid */
    {
        bool cur_has_church = (ch->church != NULL && ch->church_member != NULL);
        long cur_church_uid = cur_has_church ? ch->church->uid : 0;

        if (cur_has_church != cache->has_church || cur_church_uid != cache->church_uid)
            dirty |= SENTIENCE_DIRTY_CHURCH;
    }

    /* Race — check race uid */
    if (ch->race && ch->race->uid != cache->race_uid)
        dirty |= SENTIENCE_DIRTY_RACE;

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

    /* ── Phase 4: Char.Inventory ───────────────────────────────── */
    if (dirty & SENTIENCE_DIRTY_INVENTORY) {
        sentience_inventory_input_t inv_data;
        int vis_count = 0;
        ITERATOR it;
        OBJ_DATA *obj;

        memset(&inv_data, 0, sizeof(inv_data));

        inv_data.capacity_max_items = can_carry_n(ch);
        inv_data.capacity_max_weight = can_carry_w(ch);
        inv_data.capacity_current_weight = (int)ch->carry_weight;

        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            sentience_inventory_item_t *item;

            if (!can_see_obj(ch, obj))
                continue;
            if (inv_data.num_items >= SENTIENCE_MAX_INVENTORY)
                break;

            item = &inv_data.items[inv_data.num_items];
            item->name = obj->short_descr ? obj->short_descr : "something";
            item->keyword = obj->name ? obj->name : "";
            item->id[0] = obj->id[0];
            item->id[1] = obj->id[1];
            item->item_type = item_type_info[obj->item_type].name;
            item->level = obj->level;
            item->weight = obj->weight;
            item->condition_label = object_damage_table[URANGE(0, 9 - (int)(((float)obj->condition)/10), 9)].name;

            /* Actions */
            item->num_actions = 0;
            if (CAN_WEAR(obj, ITEM_WEAR_BODY) || CAN_WEAR(obj, ITEM_WEAR_HEAD)
                || CAN_WEAR(obj, ITEM_WEAR_LEGS) || CAN_WEAR(obj, ITEM_WEAR_FEET)
                || CAN_WEAR(obj, ITEM_WEAR_HANDS) || CAN_WEAR(obj, ITEM_WEAR_ARMS)
                || CAN_WEAR(obj, ITEM_WEAR_ABOUT) || CAN_WEAR(obj, ITEM_WEAR_WAIST)
                || CAN_WEAR(obj, ITEM_WEAR_WRIST) || CAN_WEAR(obj, ITEM_WEAR_SHIELD)
                || CAN_WEAR(obj, ITEM_WIELD) || CAN_WEAR(obj, ITEM_HOLD)
                || CAN_WEAR(obj, ITEM_WEAR_FLOAT) || CAN_WEAR(obj, ITEM_WEAR_NECK)
                || CAN_WEAR(obj, ITEM_WEAR_FINGER) || CAN_WEAR(obj, ITEM_WEAR_EAR)) {
                item->actions[item->num_actions].label = "Wear";
                item->actions[item->num_actions].cmd = "wear";
                item->num_actions++;
            }
            if (item->num_actions < SENTIENCE_MAX_ITEM_ACTIONS) {
                item->actions[item->num_actions].label = "Drop";
                item->actions[item->num_actions].cmd = "drop";
                item->num_actions++;
            }
            if (item->num_actions < SENTIENCE_MAX_ITEM_ACTIONS) {
                item->actions[item->num_actions].label = "Examine";
                item->actions[item->num_actions].cmd = "examine";
                item->num_actions++;
            }

            vis_count++;
            inv_data.num_items++;
        }
        iterator_stop(&it);

        sentience_send_package(d, "Sentience.Char.Inventory",
            sentience_build_inventory_json(&inv_data));
        cache->inventory_count = vis_count;
    }

    /* ── Phase 4: Char.Equipment ───────────────────────────────── */
    if (dirty & SENTIENCE_DIRTY_EQUIPMENT) {
        sentience_equipment_input_t eq_data;
        char *slot_names[SENTIENCE_MAX_EQUIPMENT_SLOTS];
        int worn_count = 0;
        int slot;
        int i;

        memset(&eq_data, 0, sizeof(eq_data));
        memset(slot_names, 0, sizeof(slot_names));

        for (slot = 0; slot < MAX_WEAR && eq_data.num_slots < SENTIENCE_MAX_EQUIPMENT_SLOTS; slot++) {
            sentience_equipment_slot_t *s = &eq_data.slots[eq_data.num_slots];
            OBJ_DATA *worn = get_eq_char(ch, slot);

            slot_names[eq_data.num_slots] = nocolour(where_name[slot]);
            s->slot_name = slot_names[eq_data.num_slots];
            s->slot_id = slot;

            if (worn) {
                s->occupied = true;
                s->item_name = worn->short_descr ? worn->short_descr : "something";
                s->keywords = worn->name ? worn->name : "";
                s->keyword = worn->name ? worn->name : "";
                s->id[0] = worn->id[0];
                s->id[1] = worn->id[1];
                s->item_type = item_type_info[worn->item_type].name;
                s->level = worn->level;
                s->condition_label = object_damage_table[URANGE(0, 9 - (int)(((float)worn->condition)/10), 9)].name;

                s->num_actions = 0;
                s->actions[s->num_actions].label = "Remove";
                s->actions[s->num_actions].cmd = "remove";
                s->num_actions++;
                if (s->num_actions < SENTIENCE_MAX_ITEM_ACTIONS) {
                    s->actions[s->num_actions].label = "Examine";
                    s->actions[s->num_actions].cmd = "examine";
                    s->num_actions++;
                }
                worn_count++;
            } else {
                s->occupied = false;
            }

            eq_data.num_slots++;
        }

        sentience_send_package(d, "Sentience.Char.Equipment",
            sentience_build_equipment_json(&eq_data));
        cache->equipment_count = worn_count;

        /* Free stripped slot name strings */
        for (i = 0; i < eq_data.num_slots; i++) {
            if (slot_names[i])
                free_string(slot_names[i]);
        }
    }

    /* ── Phase 4: Char.Abilities ───────────────────────────────── */
    if (dirty & SENTIENCE_DIRTY_ABILITIES) {
        sentience_abilities_input_t ab_data;
        SKILL_ENTRY *entry;

        memset(&ab_data, 0, sizeof(ab_data));

        /* Skills and spells from sorted_skills */
        for (entry = ch->sorted_skills; entry; entry = entry->next) {
            sentience_ability_t *ab;
            int rating;

            if (ab_data.num_abilities >= SENTIENCE_MAX_ABILITIES)
                break;

            rating = skill_entry_rating(ch, entry);
            if (rating < 1)
                continue;

            ab = &ab_data.abilities[ab_data.num_abilities];
            ab->name = skill_entry_name(entry);
            ab->type = entry->isspell ? "spell" : "skill";
            ab->available = skill_entry_is_usable_now(ch, entry);
            ab->rating = rating;
            ab->modifier = skill_entry_mod(ch, entry);
            ab->level = skill_entry_level(ch, entry);
            ab->mana = entry->isspell ? skill_entry_mana(ch, entry) : 0;
            ab->can_practice = entry->practice;
            ab->learn_rate = skill_entry_learn(ch, entry);

            /* Target type */
            if (entry->skill_data) {
                switch (entry->skill_data->target) {
                case TAR_CHAR_OFFENSIVE:
                    ab->target = "offensive";
                    break;
                case TAR_CHAR_DEFENSIVE:
                case TAR_OBJ_CHAR_DEF:
                case TAR_IGNORE_CHAR_DEF:
                    ab->target = "defensive";
                    break;
                case TAR_CHAR_SELF:
                    ab->target = "self";
                    break;
                case TAR_OBJ_INV:
                case TAR_OBJ_GROUND:
                    ab->target = "object";
                    break;
                case TAR_OBJ_CHAR_OFF:
                case TAR_CHAR_FORMATION:
                    ab->target = "offensive";
                    break;
                default:
                    ab->target = entry->isspell ? "ignore" : "passive";
                    break;
                }
            } else {
                ab->target = entry->isspell ? "ignore" : "passive";
            }

            /* Actions */
            ab->num_actions = 0;
            if (entry->isspell && ab->available) {
                ab->actions[ab->num_actions].label = "Cast";
                ab->actions[ab->num_actions].cmd = "cast";
                ab->num_actions++;
            } else if (!entry->isspell && ab->available) {
                ab->actions[ab->num_actions].label = "Use";
                ab->actions[ab->num_actions].cmd = ab->name;
                ab->num_actions++;
            }

            ab_data.num_abilities++;
        }

        /* Songs from sorted_songs */
        for (entry = ch->sorted_songs; entry; entry = entry->next) {
            sentience_ability_t *ab;
            int rating;

            if (ab_data.num_abilities >= SENTIENCE_MAX_ABILITIES)
                break;

            rating = skill_entry_rating(ch, entry);
            if (rating < 1)
                continue;

            ab = &ab_data.abilities[ab_data.num_abilities];
            ab->name = skill_entry_name(entry);
            ab->type = "song";
            ab->available = skill_entry_is_usable_now(ch, entry);
            ab->rating = rating;
            ab->modifier = skill_entry_mod(ch, entry);
            ab->level = skill_entry_level(ch, entry);
            ab->mana = skill_entry_mana(ch, entry);
            ab->can_practice = entry->practice;
            ab->learn_rate = skill_entry_learn(ch, entry);
            ab->target = "ignore";

            ab->num_actions = 0;
            if (ab->available) {
                ab->actions[ab->num_actions].label = "Play";
                ab->actions[ab->num_actions].cmd = "play";
                ab->num_actions++;
            }

            ab_data.num_abilities++;
        }

        sentience_send_package(d, "Sentience.Char.Abilities",
            sentience_build_abilities_json(&ab_data));
        cache->abilities_count = ab_data.num_abilities;
    }

    /* ── Phase 4: Char.Reputations ─────────────────────────────── */
    if (dirty & SENTIENCE_DIRTY_REPUTATIONS) {
        sentience_reputations_input_t rep_data;

        memset(&rep_data, 0, sizeof(rep_data));

        if (ch->reputations) {
            ITERATOR it;
            REPUTATION_DATA *rep;

            iterator_start(&it, ch->reputations);
            while ((rep = (REPUTATION_DATA *)iterator_nextdata(&it)) != NULL) {
                sentience_reputation_t *r;
                REPUTATION_INDEX_RANK_DATA *rank;
                REPUTATION_INDEX_RANK_DATA *max_rank;

                if (rep_data.num_reputations >= SENTIENCE_MAX_REPUTATIONS)
                    break;
                if (!rep->pIndexData || IS_SET(rep->pIndexData->flags, REPUTATION_HIDDEN))
                    continue;

                r = &rep_data.reputations[rep_data.num_reputations];
                r->name = rep->pIndexData->name ? rep->pIndexData->name : "(unknown)";

                rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
                r->rank = (rank && rank->name) ? rank->name : "(none)";
                r->rank_color = "";
                if (rank) {
                    /* Convert color char to color name string */
                    switch (rank->color) {
                    case 'R': r->rank_color = "red"; break;
                    case 'G': r->rank_color = "green"; break;
                    case 'B': r->rank_color = "blue"; break;
                    case 'Y': r->rank_color = "yellow"; break;
                    case 'M': r->rank_color = "magenta"; break;
                    case 'C': r->rank_color = "cyan"; break;
                    case 'W': r->rank_color = "white"; break;
                    case 'D': r->rank_color = "dark"; break;
                    default: r->rank_color = "white"; break;
                    }
                }

                r->points = (int)rep->reputation;
                r->paragon_level = rep->paragon_level;

                max_rank = get_reputation_rank(rep->pIndexData, rep->maximum_rank);
                r->max_rank = (max_rank && max_rank->name) ? max_rank->name : "(none)";

                rep_data.num_reputations++;
            }
            iterator_stop(&it);
        }

        sentience_send_package(d, "Sentience.Char.Reputations",
            sentience_build_reputations_json(&rep_data));
        cache->reputation_count = rep_data.num_reputations;
    }

    /* ── Phase 4: Char.Church ──────────────────────────────────── */
    if (dirty & SENTIENCE_DIRTY_CHURCH) {
        sentience_church_input_t church_data;

        memset(&church_data, 0, sizeof(church_data));

        if (ch->church && ch->church_member) {
            CHURCH_DATA *church = ch->church;
            CHURCH_PLAYER_DATA *member = ch->church_member;

            church_data.is_member = true;
            church_data.church_name = church->name ? church->name : "(unnamed)";
            church_data.church_flag = church->flag ? church->flag : "";
            church_data.pk = church->pk;

            /* Alignment */
            switch (church->alignment) {
            case CHURCH_GOOD:    church_data.alignment = "good"; break;
            case CHURCH_EVIL:    church_data.alignment = "evil"; break;
            default:             church_data.alignment = "neutral"; break;
            }

            /* Size */
            switch (church->size) {
            case CHURCH_SIZE_BAND:   church_data.size = "band"; break;
            case CHURCH_SIZE_CULT:   church_data.size = "cult"; break;
            case CHURCH_SIZE_ORDER:  church_data.size = "order"; break;
            case CHURCH_SIZE_CHURCH: church_data.size = "church"; break;
            default:                 church_data.size = "band"; break;
            }

            /* Rank info */
            if (member->rank) {
                church_data.rank_name = member->rank->rank_name ? member->rank->rank_name : "(none)";

                switch (member->rank->rank_type) {
                case RANK_TYPE_OFFICER: church_data.rank_type = "officer"; break;
                case RANK_TYPE_LEADER:  church_data.rank_type = "leader"; break;
                default:                church_data.rank_type = "member"; break;
                }

                /* Use gender-appropriate title */
                if (ch->sex == SEX_FEMALE && member->rank->title_female)
                    church_data.rank_title = member->rank->title_female;
                else if (ch->sex == SEX_MALE && member->rank->title_male)
                    church_data.rank_title = member->rank->title_male;
                else if (member->rank->title_neutral)
                    church_data.rank_title = member->rank->title_neutral;
                else
                    church_data.rank_title = member->rank->rank_name ? member->rank->rank_name : "";
            } else {
                church_data.rank_name = "(none)";
                church_data.rank_type = "member";
                church_data.rank_title = "";
            }

            cache->has_church = true;
            cache->church_uid = church->uid;
        } else {
            church_data.is_member = false;
            cache->has_church = false;
            cache->church_uid = 0;
        }

        sentience_send_package(d, "Sentience.Char.Church",
            sentience_build_church_json(&church_data));
    }

    /* ── Phase 4: Char.Race ────────────────────────────────────── */
    if (dirty & SENTIENCE_DIRTY_RACE) {
        if (ch->race) {
            sentience_race_info_t race_data;
            int s;

            memset(&race_data, 0, sizeof(race_data));

            race_data.id = ch->race->id ? ch->race->id : "";
            race_data.name = ch->race->name ? ch->race->name : "";
            race_data.description = ch->race->description ? ch->race->description : "";
            race_data.playable = ch->race->playable;
            race_data.starting = ch->race->starting;
            race_data.size = size_table[URANGE(0, ch->race->min_size, SIZE_GIANT)].name;

            /* Stats */
            for (s = 0; s < 5 && s < MAX_STATS; s++) {
                race_data.stats[s] = ch->race->stats[s];
                race_data.max_stats[s] = ch->race->max_stats[s];
            }

            /* Max vitals */
            race_data.max_vitals[0] = ch->race->max_vitals[0];
            race_data.max_vitals[1] = ch->race->max_vitals[1];
            race_data.max_vitals[2] = ch->race->max_vitals[2];

            /* Resistances, vulnerabilities, immunities, affects */
            race_data.resistances = ch->race->res ? flag_string(res_flags, ch->race->res) : "";
            race_data.vulnerabilities = ch->race->vuln ? flag_string(vuln_flags, ch->race->vuln) : "";
            race_data.immunities = ch->race->imm ? flag_string(imm_flags, ch->race->imm) : "";
            race_data.affects = ch->race->aff[0] ? flag_string(affect_flags, ch->race->aff[0]) : "";

            /* Remort destination */
            race_data.remort_into = ch->race->remort_into_id;

            /* Racial skills */
            race_data.num_skills = 0;
            if (ch->race->skills) {
                ITERATOR it;
                char *skill_name;

                iterator_start(&it, ch->race->skills);
                while ((skill_name = (char *)iterator_nextdata(&it)) != NULL) {
                    if (race_data.num_skills >= SENTIENCE_MAX_RACE_SKILLS)
                        break;
                    race_data.skills[race_data.num_skills] = skill_name;
                    race_data.num_skills++;
                }
                iterator_stop(&it);
            }

            sentience_send_package(d, "Sentience.Char.Race",
                sentience_build_race_json(&race_data));
        }
        cache->race_uid = ch->race ? ch->race->uid : 0;
    }

    cache->initialized = true;
}

void sentience_invalidate_cache(CHAR_DATA *ch, unsigned int flags)
{
    descriptor_t *d;

    if (!ch || !(d = ch->desc))
        return;
    if (!d->pProtocol || !d->pProtocol->bGMCP)
        return;

    if (flags & SENTIENCE_DIRTY_ABILITIES)
        d->pProtocol->sentience_cache.abilities_count = -1;
    if (flags & SENTIENCE_DIRTY_REPUTATIONS)
        d->pProtocol->sentience_cache.reputation_count = -1;
    if (flags & SENTIENCE_DIRTY_INVENTORY)
        d->pProtocol->sentience_cache.inventory_count = -1;
    if (flags & SENTIENCE_DIRTY_EQUIPMENT)
        d->pProtocol->sentience_cache.equipment_count = -1;
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

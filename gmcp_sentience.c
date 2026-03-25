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
#include "merc.h"
#include "wilds.h"
#include "gmcp_sentience.h"
#include "protocol.h"
#include "class_data.h"
#include "log.h"

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
static void sentience_send_package(descriptor_t *d, const char *package, json_t *json)
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

        cache->room_id0 = room->area ? room->area->uid : 0;
        cache->room_id1 = room->vnum;
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

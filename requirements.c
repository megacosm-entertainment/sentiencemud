#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "io/json/json_common.h"

#include "merc.h"
#include "tables.h"
#include "scripts.h"
#include "requirements.h"
#include "wilds.h"

#define REQUIREMENTS_MAX_DEPTH 16

typedef enum requirement_compare_op {
    REQUIREMENT_OP_EQ,
    REQUIREMENT_OP_NE,
    REQUIREMENT_OP_LT,
    REQUIREMENT_OP_LE,
    REQUIREMENT_OP_GT,
    REQUIREMENT_OP_GE
} REQUIREMENT_COMPARE_OP;

static bool requirements_eval_node(const json_t *node,
                                   const REQUIREMENT_CONTEXT *context,
                                   int depth);

static bool requirements_compare_int(int lhs, REQUIREMENT_COMPARE_OP op, int rhs)
{
    switch (op) {
    case REQUIREMENT_OP_EQ:
        return lhs == rhs;
    case REQUIREMENT_OP_NE:
        return lhs != rhs;
    case REQUIREMENT_OP_LT:
        return lhs < rhs;
    case REQUIREMENT_OP_LE:
        return lhs <= rhs;
    case REQUIREMENT_OP_GT:
        return lhs > rhs;
    case REQUIREMENT_OP_GE:
    default:
        return lhs >= rhs;
    }
}

static REQUIREMENT_COMPARE_OP requirements_parse_compare_op(const char *op_name,
                                                            REQUIREMENT_COMPARE_OP default_op)
{
    if (IS_NULLSTR(op_name))
        return default_op;

    if (!str_cmp(op_name, "==") || !str_cmp(op_name, "eq"))
        return REQUIREMENT_OP_EQ;
    if (!str_cmp(op_name, "!=") || !str_cmp(op_name, "ne"))
        return REQUIREMENT_OP_NE;
    if (!str_cmp(op_name, "<") || !str_cmp(op_name, "lt"))
        return REQUIREMENT_OP_LT;
    if (!str_cmp(op_name, "<=") || !str_cmp(op_name, "le"))
        return REQUIREMENT_OP_LE;
    if (!str_cmp(op_name, ">") || !str_cmp(op_name, "gt"))
        return REQUIREMENT_OP_GT;
    if (!str_cmp(op_name, ">=") || !str_cmp(op_name, "ge"))
        return REQUIREMENT_OP_GE;

    return default_op;
}

static bool requirements_eval_all_of(const json_t *array,
                                     const REQUIREMENT_CONTEXT *context,
                                     int depth)
{
    size_t i;
    json_t *item;

    if (!json_is_array(array))
        return false;

    json_array_foreach(array, i, item) {
        if (!requirements_eval_node(item, context, depth + 1))
            return false;
    }

    return true;
}

static bool requirements_eval_any_of(const json_t *array,
                                     const REQUIREMENT_CONTEXT *context,
                                     int depth)
{
    size_t i;
    json_t *item;
    bool saw_item = false;

    if (!json_is_array(array))
        return false;

    json_array_foreach(array, i, item) {
        saw_item = true;
        if (requirements_eval_node(item, context, depth + 1))
            return true;
    }

    return !saw_item;
}

static bool requirements_eval_plr_flag(const json_t *value,
                                       const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    const char *flag_name = NULL;
    bool expected = true;
    long bit;

    if (!context || !(actor = context->actor) || IS_NPC(actor))
        return false;

    if (json_is_string(value)) {
        flag_name = json_string_value(value);
    } else if (json_is_object(value)) {
        flag_name = json_get_string((json_t *)value, "name", "");
        if (json_is_boolean(json_object_get(value, "value")))
            expected = json_boolean_value(json_object_get(value, "value"));
    } else {
        return false;
    }

    if (IS_NULLSTR(flag_name))
        return false;

    bit = flag_value(plr_flags, (char *)flag_name);
    if (bit == NO_FLAG)
        return false;

    return (IS_SET(actor->act[0], bit) != 0) == expected;
}

static bool requirements_eval_staff_rank(const json_t *value,
                                         const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    int expected_rank;
    REQUIREMENT_COMPARE_OP op = REQUIREMENT_OP_GE;

    if (!context || !(actor = context->actor) || IS_NPC(actor))
        return false;

    if (json_is_integer(value)) {
        expected_rank = (int)json_integer_value(value);
    } else if (json_is_string(value)) {
        long rank_bit = flag_value(staff_ranks, (char *)json_string_value(value));
        if (rank_bit == NO_FLAG)
            return false;
        expected_rank = (int)rank_bit;
    } else if (json_is_object(value)) {
        const char *op_name = json_get_string((json_t *)value, "op", "");
        json_t *rank_value = json_object_get(value, "value");

        op = requirements_parse_compare_op(op_name, REQUIREMENT_OP_GE);
        if (json_is_integer(rank_value)) {
            expected_rank = (int)json_integer_value(rank_value);
        } else if (json_is_string(rank_value)) {
            long rank_bit = flag_value(staff_ranks, (char *)json_string_value(rank_value));
            if (rank_bit == NO_FLAG)
                return false;
            expected_rank = (int)rank_bit;
        } else {
            return false;
        }
    } else {
        return false;
    }

    return requirements_compare_int(get_staff_rank(actor), op, expected_rank);
}

static bool requirements_eval_tot_level(const json_t *value,
                                        const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    int expected_level;
    REQUIREMENT_COMPARE_OP op = REQUIREMENT_OP_GE;

    if (!context || !(actor = context->actor))
        return false;

    if (json_is_integer(value)) {
        expected_level = (int)json_integer_value(value);
    } else if (json_is_object(value)) {
        const char *op_name = json_get_string((json_t *)value, "op", "");
        json_t *level_value = json_object_get(value, "value");

        if (!json_is_integer(level_value))
            return false;

        op = requirements_parse_compare_op(op_name, REQUIREMENT_OP_GE);
        expected_level = (int)json_integer_value(level_value);
    } else {
        return false;
    }

    return requirements_compare_int(actor->tot_level, op, expected_level);
}

static bool requirements_token_matches(TOKEN_DATA *token,
                                       const WNUM *token_wnum,
                                       const char *name)
{
    if (!token)
        return false;

    if (token_wnum && token_wnum->vnum > 0) {
        if (!wnum_match_token(*token_wnum, token))
            return false;
    }

    if (!IS_NULLSTR(name)) {
        if (IS_NULLSTR(token->name) || str_cmp(token->name, name))
            return false;
    }

    return true;
}

static bool requirements_eval_token(const json_t *value,
                                    const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    WNUM_LOAD token_load = { 0, 0 };
    WNUM token_wnum = { NULL, 0 };
    bool has_token_ref = false;
    const char *name = NULL;
    int min_count = 1;
    int count = 0;
    TOKEN_DATA *token;

    if (!context || !(actor = context->actor))
        return false;

    if (json_is_integer(value)) {
        token_load.vnum = (long)json_integer_value(value);
        has_token_ref = token_load.vnum > 0;
    } else if (json_is_string(value)) {
        const char *text = json_string_value(value);
        if (parse_widevnum_load(text, &token_load))
            has_token_ref = true;
        else
            name = text;
    } else if (json_is_object(value)) {
        json_t *v;

        v = json_object_get(value, "wnum");
        if (json_is_string(v) && parse_widevnum_load(json_string_value(v), &token_load))
            has_token_ref = true;

        v = json_object_get(value, "vnum");
        if (!has_token_ref && json_is_integer(v)) {
            token_load.vnum = (long)json_integer_value(v);
            has_token_ref = token_load.vnum > 0;
        } else if (!has_token_ref && json_is_string(v) && parse_widevnum_load(json_string_value(v), &token_load)) {
            has_token_ref = true;
        }

        v = json_object_get(value, "auid");
        if (json_is_integer(v)) {
            token_load.auid = (long)json_integer_value(v);
            if (token_load.vnum > 0)
                has_token_ref = true;
        }

        v = json_object_get(value, "name");
        if (json_is_string(v))
            name = json_string_value(v);

        v = json_object_get(value, "min_count");
        if (json_is_integer(v))
            min_count = UMAX(1, (int)json_integer_value(v));
    } else {
        return false;
    }

    if (!has_token_ref && IS_NULLSTR(name))
        return false;

    if (has_token_ref) {
        AREA_DATA *fallback = NULL;

        if (actor->in_room)
            fallback = actor->in_room->area;

        resolve_wnum_load(&token_load, &token_wnum, fallback);
        if (token_wnum.vnum <= 0)
            return false;
    }

    for (token = actor->tokens; token; token = token->next) {
        if (requirements_token_matches(token, has_token_ref ? &token_wnum : NULL, name)) {
            count++;
            if (count >= min_count)
                return true;
        }
    }

    return false;
}

/*
 * requirements_eval_quest_completed - check if actor completed a v2 quest
 *
 * Value: wnum string ("auid#vnum"), bare vnum integer, or object with
 * "wnum"/"vnum" fields.
 *
 * @param value    JSON spec value
 * @param context  evaluation context
 * @return         true if the quest was completed
 */
static bool requirements_eval_quest_completed(const json_t *value,
                                               const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    QUEST_HISTORY_DATA *history;
    WNUM_LOAD wnum_load = { 0, 0 };
    bool by_wnum = false;
    long vnum_only = 0;

    if (!context || !(actor = context->actor) || IS_NPC(actor))
        return false;

    if (!actor->pcdata)
        return false;

    if (json_is_integer(value)) {
        vnum_only = (long)json_integer_value(value);
    } else if (json_is_string(value)) {
        const char *text = json_string_value(value);
        if (parse_widevnum_load(text, &wnum_load))
            by_wnum = true;
        else
            vnum_only = atol(text);
    } else if (json_is_object(value)) {
        json_t *v = json_object_get(value, "wnum");
        if (json_is_string(v) && parse_widevnum_load(json_string_value(v), &wnum_load))
            by_wnum = true;
        v = json_object_get(value, "vnum");
        if (!by_wnum && json_is_integer(v))
            vnum_only = (long)json_integer_value(v);
    } else {
        return false;
    }

    for (history = actor->pcdata->quest_history; history != NULL; history = history->next) {
        if (history->run_status != QUEST_RUN_STATUS_COMPLETED)
            continue;

        if (by_wnum) {
            if (wnum_load.auid > 0) {
                if (history->quest_index_v2_auid == wnum_load.auid
                    && history->quest_index_v2_vnum == wnum_load.vnum)
                    return true;
            } else {
                if (history->quest_index_v2_vnum == wnum_load.vnum)
                    return true;
            }
        } else {
            if (history->quest_index_v2_vnum == vnum_only)
                return true;
        }
    }

    return false;
}

/*
 * requirements_eval_quest_active - check if actor has an active v2 quest run
 *
 * Value: same format as quest_completed.
 *
 * @param value    JSON spec value
 * @param context  evaluation context
 * @return         true if the quest is currently active
 */
static bool requirements_eval_quest_active(const json_t *value,
                                           const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    QUEST_DATA *run;
    WNUM_LOAD wnum_load = { 0, 0 };
    bool by_wnum = false;
    long vnum_only = 0;

    if (!context || !(actor = context->actor) || IS_NPC(actor))
        return false;

    if (!actor->pcdata)
        return false;

    if (json_is_integer(value)) {
        vnum_only = (long)json_integer_value(value);
    } else if (json_is_string(value)) {
        const char *text = json_string_value(value);
        if (parse_widevnum_load(text, &wnum_load))
            by_wnum = true;
        else
            vnum_only = atol(text);
    } else if (json_is_object(value)) {
        json_t *v = json_object_get(value, "wnum");
        if (json_is_string(v) && parse_widevnum_load(json_string_value(v), &wnum_load))
            by_wnum = true;
        v = json_object_get(value, "vnum");
        if (!by_wnum && json_is_integer(v))
            vnum_only = (long)json_integer_value(v);
    } else {
        return false;
    }

    for (run = actor->quest; run != NULL; run = run->next) {
        if (run->run_status != QUEST_RUN_STATUS_ACTIVE)
            continue;
        if (run->generating)
            continue;

        if (by_wnum) {
            if (wnum_load.auid > 0) {
                if (run->quest_index_v2_auid == wnum_load.auid
                    && run->quest_index_v2_vnum == wnum_load.vnum)
                    return true;
            } else {
                if (run->quest_index_v2_vnum == wnum_load.vnum)
                    return true;
            }
        } else {
            if (run->quest_index_v2_vnum == vnum_only)
                return true;
        }
    }

    return false;
}

/*
 * requirements_eval_reputation - check actor's reputation rank
 *
 * Value forms:
 *   string: "auid#vnum"      — tests that rep entry exists (any rank)
 *   object: { "wnum": "auid#vnum", "rank": N, "op": ">=" }
 *
 * @param value    JSON spec value
 * @param context  evaluation context
 * @return         true if the reputation condition is met
 */
static bool requirements_eval_reputation(const json_t *value,
                                         const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    REPUTATION_DATA *rep;
    WNUM_LOAD wnum_load = { 0, 0 };
    WNUM wnum = { NULL, 0 };
    int expected_rank = 0;
    REQUIREMENT_COMPARE_OP op = REQUIREMENT_OP_GE;
    bool has_rank_check = false;

    if (!context || !(actor = context->actor) || IS_NPC(actor))
        return false;

    if (!actor->pcdata)
        return false;

    if (json_is_string(value)) {
        if (!parse_widevnum_load(json_string_value(value), &wnum_load))
            return false;
    } else if (json_is_object(value)) {
        json_t *v = json_object_get(value, "wnum");
        if (!json_is_string(v) || !parse_widevnum_load(json_string_value(v), &wnum_load))
            return false;

        v = json_object_get(value, "rank");
        if (json_is_integer(v)) {
            expected_rank = (int)json_integer_value(v);
            has_rank_check = true;
        }

        v = json_object_get(value, "op");
        if (json_is_string(v))
            op = requirements_parse_compare_op(json_string_value(v), REQUIREMENT_OP_GE);
    } else {
        return false;
    }

    {
        AREA_DATA *fallback = actor->in_room ? actor->in_room->area : NULL;
        resolve_wnum_load(&wnum_load, &wnum, fallback);
    }

    if (wnum.pArea == NULL || wnum.vnum <= 0)
        return false;

    rep = get_reputation_char_wnum(actor, wnum, false, false);
    if (!rep || !rep->valid)
        return false;

    if (!has_rank_check)
        return true;

    return requirements_compare_int((int)rep->current_rank, op, expected_rank);
}

/*
 * requirements_eval_class_current - check if named class is actor's active class
 *
 * Value: class name string.
 *
 * @param value    JSON spec value
 * @param context  evaluation context
 * @return         true if class matches actor's currently selected class
 */
static bool requirements_eval_class_current(const json_t *value,
                                            const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    const char *class_name;
    bool is_current = false;
    int level = 0;
    bool has_class = false;

    if (!context || !(actor = context->actor) || IS_NPC(actor))
        return false;

    if (!json_is_string(value))
        return false;

    class_name = json_string_value(value);
    if (IS_NULLSTR(class_name))
        return false;

    script_get_class_by_name_metrics(actor, class_name, &is_current, &level, &has_class);
    (void)level; (void)has_class;
    return is_current;
}

/*
 * requirements_eval_class_available - check if named class is unlocked
 *
 * Value: class name string.
 *
 * @param value    JSON spec value
 * @param context  evaluation context
 * @return         true if actor has any levels in the named class
 */
static bool requirements_eval_class_available(const json_t *value,
                                              const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    const char *class_name;
    bool is_current = false;
    int level = 0;
    bool has_class = false;

    if (!context || !(actor = context->actor) || IS_NPC(actor))
        return false;

    if (!json_is_string(value))
        return false;

    class_name = json_string_value(value);
    if (IS_NULLSTR(class_name))
        return false;

    script_get_class_by_name_metrics(actor, class_name, &is_current, &level, &has_class);
    (void)is_current; (void)level;
    return has_class;
}

/*
 * requirements_eval_class_level - check actor's level in a named class
 *
 * Value: { "class": "name", "level": N, "op": ">=" }
 *
 * @param value    JSON spec value
 * @param context  evaluation context
 * @return         true if actor's class level satisfies the comparison
 */
static bool requirements_eval_class_level(const json_t *value,
                                          const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    const char *class_name;
    bool is_current = false;
    int actual_level = 0;
    bool has_class = false;
    int expected_level;
    REQUIREMENT_COMPARE_OP op = REQUIREMENT_OP_GE;

    if (!context || !(actor = context->actor) || IS_NPC(actor))
        return false;

    if (!json_is_object(value))
        return false;

    {
        json_t *cn = json_object_get(value, "class");
        json_t *lv = json_object_get(value, "level");
        json_t *ov = json_object_get(value, "op");

        if (!json_is_string(cn) || !json_is_integer(lv))
            return false;

        class_name = json_string_value(cn);
        expected_level = (int)json_integer_value(lv);
        if (json_is_string(ov))
            op = requirements_parse_compare_op(json_string_value(ov), REQUIREMENT_OP_GE);
    }

    if (IS_NULLSTR(class_name))
        return false;

    script_get_class_by_name_metrics(actor, class_name, &is_current, &actual_level, &has_class);
    (void)is_current;
    if (!has_class)
        return false;

    return requirements_compare_int(actual_level, op, expected_level);
}

/*
 * requirements_eval_race - check actor's race
 *
 * Value: race string ID or display name.
 *
 * @param value    JSON spec value
 * @param context  evaluation context
 * @return         true if actor's race matches the specified race
 */
static bool requirements_eval_race(const json_t *value,
                                   const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    const char *race_name;
    RACE_DATA *target_race;

    if (!context || !(actor = context->actor))
        return false;

    if (!json_is_string(value))
        return false;

    race_name = json_string_value(value);
    if (IS_NULLSTR(race_name))
        return false;

    target_race = race_lookup(race_name);
    if (!target_race)
        target_race = race_lookup_name(race_name);

    if (!target_race)
        return false;

    return actor->race == target_race;
}

/*
 * requirements_eval_quest_points - check actor's quest point total
 *
 * Value: N (integer, >= check) or { "value": N, "op": ">=" }
 *
 * @param value    JSON spec value
 * @param context  evaluation context
 * @return         true if actor's quest points satisfy the comparison
 */
static bool requirements_eval_quest_points(const json_t *value,
                                           const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    int expected;
    REQUIREMENT_COMPARE_OP op = REQUIREMENT_OP_GE;

    if (!context || !(actor = context->actor) || IS_NPC(actor))
        return false;

    if (!actor->pcdata)
        return false;

    if (json_is_integer(value)) {
        expected = (int)json_integer_value(value);
    } else if (json_is_object(value)) {
        json_t *v  = json_object_get(value, "value");
        json_t *ov = json_object_get(value, "op");
        if (!json_is_integer(v))
            return false;
        expected = (int)json_integer_value(v);
        if (json_is_string(ov))
            op = requirements_parse_compare_op(json_string_value(ov), REQUIREMENT_OP_GE);
    } else {
        return false;
    }

    return requirements_compare_int((int)actor->questpoints, op, expected);
}

static ROOM_INDEX_DATA *requirements_context_room(const REQUIREMENT_CONTEXT *context)
{
    if (!context)
        return NULL;

    if (context->self_room)
        return context->self_room;

    if (context->actor)
        return context->actor->in_room;

    return NULL;
}

static bool requirements_parse_long_text(const char *text, long *out)
{
    char *endptr = NULL;
    long value;

    if (IS_NULLSTR(text) || !out)
        return false;

    value = strtol(text, &endptr, 10);
    if (!endptr || endptr == text || *endptr != '\0')
        return false;

    *out = value;
    return true;
}

static void requirements_normalize_label(const char *src, char *dst, size_t dst_sz)
{
    size_t i = 0;

    if (!dst || dst_sz == 0)
        return;

    dst[0] = '\0';
    if (IS_NULLSTR(src))
        return;

    while (src[i] != '\0' && i + 1 < dst_sz) {
        dst[i] = (src[i] == '_') ? ' ' : src[i];
        i++;
    }

    dst[i] = '\0';
}

static bool requirements_area_region_name_matches(const AREA_REGION *region,
                                                  const char *expected_name)
{
    char expected[MIL];
    char actual[MIL];

    if (!region || IS_NULLSTR(region->name) || IS_NULLSTR(expected_name))
        return false;

    requirements_normalize_label(expected_name, expected, sizeof(expected));
    requirements_normalize_label(region->name, actual, sizeof(actual));

    return !str_cmp(actual, expected);
}

static bool requirements_eval_area_region(const json_t *value,
                                          const REQUIREMENT_CONTEXT *context)
{
    ROOM_INDEX_DATA *room;
    AREA_REGION *region;
    bool invert = false;

    room = requirements_context_room(context);
    region = get_room_region(room);
    if (!room || !room->area || !region)
        return false;

    if (json_is_integer(value)) {
        return region->uid == (long)json_integer_value(value);
    }

    if (json_is_string(value)) {
        const char *text = json_string_value(value);
        long uid = 0;

        if (requirements_parse_long_text(text, &uid))
            return region->uid == uid;

        return requirements_area_region_name_matches(region, text);
    }

    if (json_is_object(value)) {
        json_t *uidv = json_object_get(value, "uid");
        json_t *namev = json_object_get(value, "name");
        json_t *ov = json_object_get(value, "op");
        json_t *vv = json_object_get(value, "value");
        bool match = false;

        if (json_is_string(ov))
            invert = !str_cmp(json_string_value(ov), "!=") || !str_cmp(json_string_value(ov), "ne");

        if (!uidv && !namev) {
            if (json_is_integer(vv))
                uidv = vv;
            else if (json_is_string(vv))
                namev = vv;
        }

        if (json_is_integer(uidv)) {
            match = (region->uid == (long)json_integer_value(uidv));
            return invert ? !match : match;
        }

        if (json_is_string(uidv)) {
            long uid = 0;
            if (requirements_parse_long_text(json_string_value(uidv), &uid)) {
                match = (region->uid == uid);
                return invert ? !match : match;
            }
        }

        if (json_is_string(namev)) {
            match = requirements_area_region_name_matches(region, json_string_value(namev));
            return invert ? !match : match;
        }

        return false;
    }

    return false;
}

static int requirements_parse_wilds_region_value(const char *text)
{
    long numeric = 0;
    char normalized[MIL];
    int region;

    if (IS_NULLSTR(text))
        return REGION_UNKNOWN;

    if (requirements_parse_long_text(text, &numeric))
        return (int)numeric;

    requirements_normalize_label(text, normalized, sizeof(normalized));
    region = (int)flag_value(wilderness_regions, normalized);
    if (region == NO_FLAG)
        return REGION_UNKNOWN;

    return region;
}

static bool requirements_wilds_box_name_matches(const WILDS_REGION *region,
                                                const char *expected_name)
{
    char expected[MIL];
    char actual[MIL];

    if (!region || IS_NULLSTR(region->name) || IS_NULLSTR(expected_name))
        return false;

    requirements_normalize_label(expected_name, expected, sizeof(expected));
    requirements_normalize_label(region->name, actual, sizeof(actual));

    return !str_cmp(actual, expected);
}

static bool requirements_eval_wilds_region(const json_t *value,
                                           const REQUIREMENT_CONTEXT *context)
{
    ROOM_INDEX_DATA *room;
    WILDS_REGION *box_region;
    int rel_x;
    int rel_y;
    bool invert = false;
    bool match = false;

    room = requirements_context_room(context);
    if (!room || !room->wilds)
        return false;

    rel_x = room->x - room->wilds->startx;
    rel_y = room->y - room->wilds->starty;
    box_region = get_region_by_coors(room->wilds, rel_x, rel_y);
    if (!box_region)
        return false;

    if (json_is_integer(value)) {
        return box_region->uid == (long)json_integer_value(value);
    }

    if (json_is_string(value)) {
        const char *text = json_string_value(value);
        long uid = 0;
        int region_value;

        if (requirements_parse_long_text(text, &uid))
            return box_region->uid == uid;

        region_value = requirements_parse_wilds_region_value(text);
        if (region_value != REGION_UNKNOWN || !str_cmp(text, "Unknown"))
            return box_region->region == region_value;

        return requirements_wilds_box_name_matches(box_region, text);
    }

    if (json_is_object(value)) {
        json_t *ov = json_object_get(value, "op");
        json_t *vv = json_object_get(value, "value");
        json_t *uidv = json_object_get(value, "uid");
        json_t *namev = json_object_get(value, "name");
        json_t *regionv = json_object_get(value, "region");

        if (json_is_string(ov))
            invert = !str_cmp(json_string_value(ov), "!=") || !str_cmp(json_string_value(ov), "ne");

        if (!uidv && !namev && !regionv)
            uidv = vv;

        if (json_is_integer(uidv)) {
            match = (box_region->uid == (long)json_integer_value(uidv));
            return invert ? !match : match;
        }

        if (json_is_string(uidv)) {
            long uid = 0;
            if (requirements_parse_long_text(json_string_value(uidv), &uid)) {
                match = (box_region->uid == uid);
                return invert ? !match : match;
            }
        }

        if (json_is_string(namev)) {
            match = requirements_wilds_box_name_matches(box_region, json_string_value(namev));
            return invert ? !match : match;
        }

        if (json_is_integer(regionv)) {
            match = (box_region->region == (int)json_integer_value(regionv));
            return invert ? !match : match;
        }

        if (json_is_string(regionv)) {
            int region_value = requirements_parse_wilds_region_value(json_string_value(regionv));
            if (region_value == REGION_UNKNOWN && str_cmp(json_string_value(regionv), "Unknown"))
                return false;

            match = (box_region->region == region_value);
            return invert ? !match : match;
        }

        return false;
    }

    return false;
}

static int requirements_storm_type_from_name(const char *name)
{
    if (IS_NULLSTR(name))
        return WEATHER_NONE;

    if (!str_cmp(name, "none"))
        return WEATHER_NONE;
    if (!str_prefix(name, "rain"))
        return WEATHER_RAIN_STORM;
    if (!str_prefix(name, "lightning") || !str_prefix(name, "storm"))
        return WEATHER_LIGHTNING_STORM;
    if (!str_prefix(name, "snow"))
        return WEATHER_SNOW_STORM;
    if (!str_prefix(name, "hurricane"))
        return WEATHER_HURRICANE;
    if (!str_prefix(name, "tornado"))
        return WEATHER_TORNADO;

    return -1;
}

static bool requirements_eval_terrain(const json_t *value,
                                      const REQUIREMENT_CONTEXT *context)
{
    ROOM_INDEX_DATA *room;
    WILDS_DATA *wilds;
    int rel_x;
    int rel_y;
    WILDS_TERRAIN *terrain;
    const char *name = NULL;
    int token = -1;
    bool invert = false;

    room = requirements_context_room(context);
    if (!room || !(wilds = room->wilds))
        return false;

    rel_x = room->x - wilds->startx;
    rel_y = room->y - wilds->starty;
    terrain = get_terrain_by_coors(wilds, rel_x, rel_y);
    if (!terrain)
        return false;

    if (json_is_string(value)) {
        const char *text = json_string_value(value);
        if (!IS_NULLSTR(text) && strlen(text) == 1)
            token = (unsigned char)text[0];
        else
            name = text;
    } else if (json_is_integer(value)) {
        token = (int)json_integer_value(value);
    } else if (json_is_object(value)) {
        json_t *tv = json_object_get(value, "token");
        json_t *nv = json_object_get(value, "name");
        json_t *ov = json_object_get(value, "op");

        if (json_is_string(ov))
            invert = !str_cmp(json_string_value(ov), "!=") || !str_cmp(json_string_value(ov), "ne");

        if (json_is_string(tv)) {
            const char *text = json_string_value(tv);
            if (!IS_NULLSTR(text) && strlen(text) == 1)
                token = (unsigned char)text[0];
            else
                return false;
        } else if (json_is_integer(tv)) {
            token = (int)json_integer_value(tv);
        } else if (json_is_string(nv)) {
            name = json_string_value(nv);
        } else if (json_is_string(json_object_get(value, "value"))) {
            const char *text = json_get_string((json_t *)value, "value", "");
            if (!IS_NULLSTR(text) && strlen(text) == 1)
                token = (unsigned char)text[0];
            else
                name = text;
        } else {
            return false;
        }
    } else {
        return false;
    }

    if (token >= 0)
        return ((terrain->mapchar == (char)token) != 0) != invert;

    if (!IS_NULLSTR(name)) {
        bool matches = false;

        if (!IS_NULLSTR(terrain->showname) && !str_cmp(name, terrain->showname))
            matches = true;
        else if (!IS_NULLSTR(terrain->briefdesc) && !str_cmp(name, terrain->briefdesc))
            matches = true;

        return matches != invert;
    }

    return false;
}

static bool requirements_eval_sector(const json_t *value,
                                     const REQUIREMENT_CONTEXT *context)
{
    ROOM_INDEX_DATA *room;
    int actual;
    int expected;
    REQUIREMENT_COMPARE_OP op = REQUIREMENT_OP_EQ;

    room = requirements_context_room(context);
    if (!room)
        return false;

    actual = room_sector_type(room);

    if (json_is_integer(value)) {
        expected = (int)json_integer_value(value);
    } else if (json_is_string(value)) {
        expected = (int)flag_value(sector_flags, (char *)json_string_value(value));
        if (expected == NO_FLAG)
            return false;
    } else if (json_is_object(value)) {
        json_t *vv = json_object_get(value, "value");
        json_t *ov = json_object_get(value, "op");

        if (json_is_string(ov))
            op = requirements_parse_compare_op(json_string_value(ov), REQUIREMENT_OP_EQ);

        if (json_is_integer(vv))
            expected = (int)json_integer_value(vv);
        else if (json_is_string(vv)) {
            expected = (int)flag_value(sector_flags, (char *)json_string_value(vv));
            if (expected == NO_FLAG)
                return false;
        } else {
            return false;
        }
    } else {
        return false;
    }

    return requirements_compare_int(actual, op, expected);
}

static bool requirements_eval_storm(const json_t *value,
                                    const REQUIREMENT_CONTEXT *context)
{
    ROOM_INDEX_DATA *room;
    int actual;
    int expected;
    REQUIREMENT_COMPARE_OP op = REQUIREMENT_OP_EQ;

    room = requirements_context_room(context);
    if (!room)
        return false;

    actual = get_storm_for_room(room);

    if (json_is_boolean(value))
        return json_boolean_value(value) ? (actual != WEATHER_NONE) : (actual == WEATHER_NONE);

    if (json_is_integer(value)) {
        expected = (int)json_integer_value(value);
        return requirements_compare_int(actual, op, expected);
    }

    if (json_is_string(value)) {
        const char *text = json_string_value(value);

        if (!str_cmp(text, "any") || !str_cmp(text, "active"))
            return actual != WEATHER_NONE;

        expected = requirements_storm_type_from_name(text);
        if (expected < 0)
            return false;
        return requirements_compare_int(actual, op, expected);
    }

    if (json_is_object(value)) {
        json_t *active = json_object_get(value, "active");
        json_t *type = json_object_get(value, "type");
        json_t *raw = json_object_get(value, "value");
        json_t *ov = json_object_get(value, "op");

        if (json_is_boolean(active))
            return json_boolean_value(active) ? (actual != WEATHER_NONE) : (actual == WEATHER_NONE);

        if (json_is_string(ov))
            op = requirements_parse_compare_op(json_string_value(ov), REQUIREMENT_OP_EQ);

        if (!type)
            type = raw;

        if (json_is_integer(type))
            expected = (int)json_integer_value(type);
        else if (json_is_string(type)) {
            expected = requirements_storm_type_from_name(json_string_value(type));
            if (expected < 0)
                return false;
        } else {
            return false;
        }

        return requirements_compare_int(actual, op, expected);
    }

    return false;
}

/*
 * requirements_eval_script - fire a prog trigger on the owning entity
 *
 * Fires p_percent_trigger on whichever self_* field is populated in the
 * context. The actor (the person being tested) becomes the "ch" argument.
 * The trigger type is chosen automatically based on the self entity:
 *   self_obj   → TRIG_PREWEAR
 *   self_mob / self_room / self_token → TRIG_PREQUEST
 *
 * Value: phrase string  OR  { "phrase": "..." }
 * Returns true if p_percent_trigger returns > 0.
 *
 * @param value    JSON spec value
 * @param context  evaluation context
 * @return         true if the script evaluation passes
 */
static bool requirements_eval_script(const json_t *value,
                                     const REQUIREMENT_CONTEXT *context)
{
    CHAR_DATA *actor;
    const char *phrase = NULL;
    int ret;

    if (!context || !(actor = context->actor))
        return false;

    if (json_is_string(value)) {
        phrase = json_string_value(value);
    } else if (json_is_object(value)) {
        json_t *ph = json_object_get(value, "phrase");
        if (json_is_string(ph))
            phrase = json_string_value(ph);
    } else {
        return false;
    }

    if (context->self_obj) {
        ret = p_percent_trigger(NULL, context->self_obj, NULL, NULL,
                                actor, NULL, NULL, NULL, NULL,
                                TRIG_PREWEAR, (char *)phrase);
        return ret > 0;
    }

    if (context->self_mob) {
        ret = p_percent_trigger(context->self_mob, NULL, NULL, NULL,
                                actor, NULL, NULL, NULL, NULL,
                                TRIG_PREQUEST, (char *)phrase);
        return ret > 0;
    }

    if (context->self_room) {
        ret = p_percent_trigger(NULL, NULL, context->self_room, NULL,
                                actor, NULL, NULL, NULL, NULL,
                                TRIG_PREQUEST, (char *)phrase);
        return ret > 0;
    }

    if (context->self_token) {
        ret = p_percent_trigger(NULL, NULL, NULL, context->self_token,
                                actor, NULL, NULL, NULL, NULL,
                                TRIG_PREQUEST, (char *)phrase);
        return ret > 0;
    }

    return false;
}

static bool requirements_eval_leaf(const char *key,
                                   const json_t *value,
                                   const REQUIREMENT_CONTEXT *context)
{
    if (IS_NULLSTR(key))
        return false;

    if (!str_cmp(key, "plr_flag"))
        return requirements_eval_plr_flag(value, context);
    if (!str_cmp(key, "staff_rank"))
        return requirements_eval_staff_rank(value, context);
    if (!str_cmp(key, "tot_level"))
        return requirements_eval_tot_level(value, context);
    if (!str_cmp(key, "token"))
        return requirements_eval_token(value, context);
    if (!str_cmp(key, "quest_completed"))
        return requirements_eval_quest_completed(value, context);
    if (!str_cmp(key, "quest_active"))
        return requirements_eval_quest_active(value, context);
    if (!str_cmp(key, "reputation"))
        return requirements_eval_reputation(value, context);
    if (!str_cmp(key, "class_current"))
        return requirements_eval_class_current(value, context);
    if (!str_cmp(key, "class_available"))
        return requirements_eval_class_available(value, context);
    if (!str_cmp(key, "class_level"))
        return requirements_eval_class_level(value, context);
    if (!str_cmp(key, "race"))
        return requirements_eval_race(value, context);
    if (!str_cmp(key, "quest_points"))
        return requirements_eval_quest_points(value, context);
    if (!str_cmp(key, "terrain"))
        return requirements_eval_terrain(value, context);
    if (!str_cmp(key, "sector"))
        return requirements_eval_sector(value, context);
    if (!str_cmp(key, "storm"))
        return requirements_eval_storm(value, context);
    if (!str_cmp(key, "area_region"))
        return requirements_eval_area_region(value, context);
    if (!str_cmp(key, "wilds_region"))
        return requirements_eval_wilds_region(value, context);
    if (!str_cmp(key, "script"))
        return requirements_eval_script(value, context);

    return false;
}

static bool requirements_eval_node(const json_t *node,
                                   const REQUIREMENT_CONTEXT *context,
                                   int depth)
{
    const char *key;
    json_t *value;
    bool has_known = false;
    bool result = true;

    if (!node)
        return true;

    if (depth > REQUIREMENTS_MAX_DEPTH)
        return false;

    if (!json_is_object(node))
        return false;

    value = json_object_get(node, "all_of");
    if (value) {
        has_known = true;
        result = result && requirements_eval_all_of(value, context, depth);
    }

    value = json_object_get(node, "any_of");
    if (value) {
        has_known = true;
        result = result && requirements_eval_any_of(value, context, depth);
    }

    json_object_foreach((json_t *)node, key, value) {
        if (!str_cmp(key, "all_of") || !str_cmp(key, "any_of"))
            continue;

        if (!str_cmp(key, "plr_flag") ||
            !str_cmp(key, "staff_rank") ||
            !str_cmp(key, "tot_level") ||
            !str_cmp(key, "token") ||
            !str_cmp(key, "quest_completed") ||
            !str_cmp(key, "quest_active") ||
            !str_cmp(key, "reputation") ||
            !str_cmp(key, "class_current") ||
            !str_cmp(key, "class_available") ||
            !str_cmp(key, "class_level") ||
            !str_cmp(key, "race") ||
            !str_cmp(key, "quest_points") ||
            !str_cmp(key, "terrain") ||
            !str_cmp(key, "sector") ||
            !str_cmp(key, "storm") ||
            !str_cmp(key, "area_region") ||
            !str_cmp(key, "wilds_region") ||
            !str_cmp(key, "script")) {
            has_known = true;
            result = result && requirements_eval_leaf(key, value, context);
            continue;
        }

        /* player_string and hidden are display-only annotations; skip silently */
        if (!str_cmp(key, "player_string") || !str_cmp(key, "hidden"))
            continue;

        return false;
    }

    if (!has_known)
        return false;

    return result;
}

/* ================================================================
 * Requirements DSL  —  text <-> JSON  (builder-friendly layer)
 *
 * Syntax:
 *   atom:
 *     tot_level [op] N
 *     staff_rank [op] N_or_name
 *     quest_points [op] N
 *     race WORD
 *     class_current WORD
 *     class_available WORD
 *     plr_flag WORD [true|false]
 *     class_level CLASS [op] N
 *     token WNUM [count N]
 *     quest_completed WNUM
 *     quest_active WNUM
 *     reputation WNUM [rank [op] N]
 *     terrain TOKEN_OR_NAME
 *     sector [op] SECTOR
 *     storm [op] TYPE | storm any | storm none
 *     area_region [op] UID_OR_NAME
 *     wilds_region [op] REGION_NAME_OR_ID
 *     script [PHRASE]
 *
 *   combinators (AND binds tighter than OR):
 *     A AND B           -> {"all_of": [A, B]}
 *     A OR  B           -> {"any_of": [A, B]}
 *     (A OR B) AND C    -> nested groups with explicit precedence
 *
 *   op = >= | <= | > | < | != | ==
 *   WNUM = widevnum string like 2#50 or bare integer
 * ================================================================ */

/* ---- Tokeniser -------------------------------------------------- */

typedef enum {
    RTOK_EOF,
    RTOK_AND,
    RTOK_OR,
    RTOK_LPAREN,
    RTOK_RPAREN,
    RTOK_OP,    /* >= <= > < != == */
    RTOK_WORD   /* identifiers, numbers, widevnums (2#50) */
} RTokType;

#define RTOK_MAX 128

typedef struct {
    RTokType type;
    char     text[RTOK_MAX];
    bool     is_number;
    long     ival;
} RTok;

static void rlex(const char **pp, RTok *t)
{
    const char *p = *pp;
    int         i;

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;

    t->is_number = false;
    t->ival      = 0;
    t->text[0]   = '\0';

    if (*p == '\0') { t->type = RTOK_EOF;    *pp = p;     return; }
    if (*p == '(')  { t->type = RTOK_LPAREN; *pp = p + 1; return; }
    if (*p == ')')  { t->type = RTOK_RPAREN; *pp = p + 1; return; }

    /* Two-char operators: >= <= != == */
    if ((*p == '>' || *p == '<' || *p == '!' || *p == '=') && *(p+1) == '=') {
        t->type    = RTOK_OP;
        t->text[0] = *p++;
        t->text[1] = *p++;
        t->text[2] = '\0';
        *pp = p;
        return;
    }

    /* Single-char operators: > < */
    if (*p == '>' || *p == '<') {
        t->type    = RTOK_OP;
        t->text[0] = *p++;
        t->text[1] = '\0';
        *pp = p;
        return;
    }

    /* Word: letters, digits, underscore, # (for widevnums like 2#50) */
    if (isalnum((unsigned char)*p) || *p == '_' || *p == '#') {
        i = 0;
        while (i < RTOK_MAX - 1 &&
               (isalnum((unsigned char)*p) || *p == '_' || *p == '#'))
            t->text[i++] = *p++;
        t->text[i] = '\0';
        *pp = p;

        if (!str_cmp(t->text, "AND")) { t->type = RTOK_AND; return; }
        if (!str_cmp(t->text, "OR"))  { t->type = RTOK_OR;  return; }

        t->type      = RTOK_WORD;
        t->is_number = (i > 0);
        for (int j = 0; j < i; j++) {
            if (!isdigit((unsigned char)t->text[j])) {
                t->is_number = false;
                break;
            }
        }
        if (t->is_number)
            t->ival = atol(t->text);
        return;
    }

    /* Unrecognised: skip */
    t->type = RTOK_EOF;
    *pp     = p + 1;
}

/* ---- Parser state ----------------------------------------------- */

#define REQ_MAX_VTOKS 16

typedef struct {
    const char *p;
    RTok        cur;
    bool        ok;
    char        err[256];
} RParser;

static void rp_advance(RParser *rp)
{
    rlex(&rp->p, &rp->cur);
}

static void rp_init(RParser *rp, const char *text)
{
    rp->p      = text;
    rp->ok     = true;
    rp->err[0] = '\0';
    rp_advance(rp);
}

static RTok rp_eat(RParser *rp)
{
    RTok t = rp->cur;
    rp_advance(rp);
    return t;
}

/* ---- Atom JSON builders ----------------------------------------- */

/*
 * req_build_int_op - build JSON for keys whose value is [op] integer
 * Used by: tot_level, quest_points
 */
static json_t *req_build_int_op(const char *key, RTok *vt, int nv,
                                char *err, size_t esz)
{
    int         idx    = 0;
    const char *op_str = NULL;
    long        n;
    json_t     *obj;

    if (nv == 0) {
        snprintf(err, esz, "%s: expected a number", key);
        return NULL;
    }

    if (vt[idx].type == RTOK_OP)
        op_str = vt[idx++].text;

    if (idx >= nv || !vt[idx].is_number) {
        snprintf(err, esz, "%s: expected integer%s",
                 key, op_str ? " after operator" : "");
        return NULL;
    }
    n = vt[idx++].ival;

    if (idx != nv) {
        snprintf(err, esz, "%s: unexpected extra tokens", key);
        return NULL;
    }

    if (op_str == NULL)
        return json_pack("{si}", key, (int)n);

    obj = json_object();
    json_object_set_new(obj, "op",    json_string(op_str));
    json_object_set_new(obj, "value", json_integer(n));
    return json_pack("{so*}", key, obj);
}

/*
 * req_build_staff_rank - build JSON for staff_rank
 * Accepts: [op] integer-or-name
 */
static json_t *req_build_staff_rank(RTok *vt, int nv, char *err, size_t esz)
{
    int         idx    = 0;
    const char *op_str = NULL;
    json_t     *obj;
    json_t     *val_json;

    if (nv == 0) {
        snprintf(err, esz, "staff_rank: expected rank value");
        return NULL;
    }

    if (vt[idx].type == RTOK_OP)
        op_str = vt[idx++].text;

    if (idx >= nv || vt[idx].type != RTOK_WORD) {
        snprintf(err, esz, "staff_rank: expected rank value%s",
                 op_str ? " after operator" : "");
        return NULL;
    }
    if (idx + 1 != nv) {
        snprintf(err, esz, "staff_rank: unexpected extra tokens");
        return NULL;
    }

    val_json = vt[idx].is_number ? json_integer(vt[idx].ival)
                                 : json_string(vt[idx].text);

    if (op_str == NULL)
        return json_pack("{so*}", "staff_rank", val_json);

    obj = json_object();
    json_object_set_new(obj, "op",    json_string(op_str));
    json_object_set_new(obj, "value", val_json);
    return json_pack("{so*}", "staff_rank", obj);
}

/*
 * req_build_word_key - build JSON for keys whose value is a single word
 * Used by: race, class_current, class_available
 */
static json_t *req_build_word_key(const char *key, RTok *vt, int nv,
                                  char *err, size_t esz)
{
    if (nv != 1 || vt[0].type != RTOK_WORD) {
        snprintf(err, esz, "%s: expected a single word", key);
        return NULL;
    }
    return json_pack("{ss}", key, vt[0].text);
}

/*
 * req_build_word_or_num_op_key - build JSON for keys with optional [op] scalar
 * Used by: sector, storm, terrain
 */
static json_t *req_build_word_or_num_op_key(const char *key, RTok *vt, int nv,
                                            char *err, size_t esz)
{
    int idx = 0;
    const char *op = NULL;
    json_t *obj;
    json_t *value;

    if (nv < 1) {
        snprintf(err, esz, "%s: expected value", key);
        return NULL;
    }

    if (vt[idx].type == RTOK_OP)
        op = vt[idx++].text;

    if (idx >= nv || vt[idx].type != RTOK_WORD) {
        snprintf(err, esz, "%s: expected value", key);
        return NULL;
    }

    if (idx + 1 != nv) {
        snprintf(err, esz, "%s: unexpected extra tokens", key);
        return NULL;
    }

    value = vt[idx].is_number ? json_integer(vt[idx].ival) : json_string(vt[idx].text);

    if (!op)
        return json_pack("{so*}", key, value);

    obj = json_object();
    json_object_set_new(obj, "op", json_string(op));
    json_object_set_new(obj, "value", value);
    return json_pack("{so*}", key, obj);
}

/*
 * req_build_plr_flag - build JSON for plr_flag
 * Accepts: WORD [true|false]
 */
static json_t *req_build_plr_flag(RTok *vt, int nv, char *err, size_t esz)
{
    json_t *obj;
    bool    val;

    if (nv == 0 || vt[0].type != RTOK_WORD) {
        snprintf(err, esz, "plr_flag: expected flag name");
        return NULL;
    }

    if (nv == 1)
        return json_pack("{ss}", "plr_flag", vt[0].text);

    if (nv != 2 || vt[1].type != RTOK_WORD) {
        snprintf(err, esz, "plr_flag: expected 'FLAG' or 'FLAG true|false'");
        return NULL;
    }

    if (!str_cmp(vt[1].text, "true"))
        val = true;
    else if (!str_cmp(vt[1].text, "false"))
        val = false;
    else {
        snprintf(err, esz, "plr_flag: expected 'true' or 'false', got '%s'",
                 vt[1].text);
        return NULL;
    }

    obj = json_object();
    json_object_set_new(obj, "name",  json_string(vt[0].text));
    json_object_set_new(obj, "value", val ? json_true() : json_false());
    return json_pack("{so*}", "plr_flag", obj);
}

/*
 * req_build_class_level - build JSON for class_level
 * Accepts: CLASS [op] N
 */
static json_t *req_build_class_level(RTok *vt, int nv, char *err, size_t esz)
{
    int         idx        = 0;
    const char *class_name;
    const char *op_str     = NULL;
    long        level;
    json_t     *obj;

    if (nv < 2) {
        snprintf(err, esz,
                 "class_level: expected 'class_level CLASS N' or 'CLASS OP N'");
        return NULL;
    }

    if (vt[idx].type != RTOK_WORD || vt[idx].is_number) {
        snprintf(err, esz, "class_level: expected class name first");
        return NULL;
    }
    class_name = vt[idx++].text;

    if (idx < nv && vt[idx].type == RTOK_OP)
        op_str = vt[idx++].text;

    if (idx >= nv || !vt[idx].is_number) {
        snprintf(err, esz, "class_level: expected level number");
        return NULL;
    }
    level = vt[idx++].ival;

    if (idx != nv) {
        snprintf(err, esz, "class_level: unexpected extra tokens");
        return NULL;
    }

    obj = json_object();
    json_object_set_new(obj, "class", json_string(class_name));
    json_object_set_new(obj, "level", json_integer(level));
    if (op_str)
        json_object_set_new(obj, "op", json_string(op_str));
    return json_pack("{so*}", "class_level", obj);
}

/*
 * req_build_token - build JSON for token
 * Accepts: WNUM [count N]
 */
static json_t *req_build_token(RTok *vt, int nv, char *err, size_t esz)
{
    json_t *obj;

    if (nv == 0 || vt[0].type != RTOK_WORD) {
        snprintf(err, esz, "token: expected wnum or token name");
        return NULL;
    }

    if (nv == 1)
        return json_pack("{ss}", "token", vt[0].text);

    /* token WNUM count N */
    if (nv == 3 &&
        vt[1].type == RTOK_WORD && !str_cmp(vt[1].text, "count") &&
        vt[2].is_number) {
        obj = json_object();
        json_object_set_new(obj, "wnum",      json_string(vt[0].text));
        json_object_set_new(obj, "min_count", json_integer(vt[2].ival));
        return json_pack("{so*}", "token", obj);
    }

    snprintf(err, esz,
             "token: expected 'token WNUM' or 'token WNUM count N'");
    return NULL;
}

/*
 * req_build_quest_ref - build JSON for quest_completed / quest_active
 * Accepts: WNUM or integer
 */
static json_t *req_build_quest_ref(const char *key, RTok *vt, int nv,
                                   char *err, size_t esz)
{
    if (nv != 1 || vt[0].type != RTOK_WORD) {
        snprintf(err, esz, "%s: expected wnum or quest number", key);
        return NULL;
    }
    if (vt[0].is_number)
        return json_pack("{si}", key, (int)vt[0].ival);
    return json_pack("{ss}", key, vt[0].text);
}

/*
 * req_build_reputation - build JSON for reputation
 * Accepts: WNUM [rank [op] N]
 */
static json_t *req_build_reputation(RTok *vt, int nv, char *err, size_t esz)
{
    json_t     *obj;
    int         idx    = 0;
    const char *op_str = NULL;
    long        rank_val;

    if (nv == 0 || vt[0].type != RTOK_WORD) {
        snprintf(err, esz, "reputation: expected wnum");
        return NULL;
    }

    if (nv == 1)
        return json_pack("{ss}", "reputation", vt[0].text);

    /* reputation WNUM rank [op] N */
    if (nv >= 3 &&
        vt[1].type == RTOK_WORD && !str_cmp(vt[1].text, "rank")) {
        idx = 2;

        if (idx < nv && vt[idx].type == RTOK_OP)
            op_str = vt[idx++].text;

        if (idx >= nv || !vt[idx].is_number) {
            snprintf(err, esz, "reputation: expected rank number");
            return NULL;
        }
        rank_val = vt[idx++].ival;

        if (idx != nv) {
            snprintf(err, esz, "reputation: unexpected extra tokens");
            return NULL;
        }

        obj = json_object();
        json_object_set_new(obj, "wnum", json_string(vt[0].text));
        json_object_set_new(obj, "rank", json_integer(rank_val));
        if (op_str)
            json_object_set_new(obj, "op", json_string(op_str));
        return json_pack("{so*}", "reputation", obj);
    }

    snprintf(err, esz,
             "reputation: expected 'reputation WNUM' or "
             "'reputation WNUM rank [OP] N'");
    return NULL;
}

/*
 * req_build_script - build JSON for script
 * Accepts: [PHRASE]
 */
static json_t *req_build_script(RTok *vt, int nv,
                                char *err __attribute__((unused)),
                                size_t esz __attribute__((unused)))
{
    if (nv == 0)
        return json_pack("{sb}", "script", 1);
    if (nv == 1 && vt[0].type == RTOK_WORD)
        return json_pack("{ss}", "script", vt[0].text);
    return json_pack("{ss}", "script", vt[0].text);
}

/* ---- Recursive descent parser ----------------------------------- */

/* Forward declaration */
static json_t *rp_parse_expr(RParser *rp);

/* Is the current token a value token (not a combinator / bracket)? */
static bool rtok_is_value(const RTok *t)
{
    return t->type == RTOK_WORD || t->type == RTOK_OP;
}

/*
 * rp_parse_atom - parse KEY vtoks... and return the JSON object for it
 */
static json_t *rp_parse_atom(RParser *rp)
{
    RTok    key_tok;
    RTok    vt[REQ_MAX_VTOKS];
    int     nv     = 0;
    char    err[256];
    json_t *result = NULL;

    if (rp->cur.type != RTOK_WORD) {
        snprintf(rp->err, sizeof(rp->err),
                 "Expected requirement keyword, got '%s'",
                 rp->cur.type == RTOK_EOF ? "(end of input)" : rp->cur.text);
        rp->ok = false;
        return NULL;
    }

    key_tok = rp_eat(rp);

    /* Collect value tokens until combinator or bracket */
    while (rp->ok && rtok_is_value(&rp->cur) && nv < REQ_MAX_VTOKS)
        vt[nv++] = rp_eat(rp);

    err[0] = '\0';
    const char *key = key_tok.text;

    if (!str_cmp(key, "tot_level") || !str_cmp(key, "quest_points"))
        result = req_build_int_op(key, vt, nv, err, sizeof(err));
    else if (!str_cmp(key, "staff_rank"))
        result = req_build_staff_rank(vt, nv, err, sizeof(err));
    else if (!str_cmp(key, "race")
             || !str_cmp(key, "class_current")
             || !str_cmp(key, "class_available"))
        result = req_build_word_key(key, vt, nv, err, sizeof(err));
    else if (!str_cmp(key, "plr_flag"))
        result = req_build_plr_flag(vt, nv, err, sizeof(err));
    else if (!str_cmp(key, "class_level"))
        result = req_build_class_level(vt, nv, err, sizeof(err));
    else if (!str_cmp(key, "token"))
        result = req_build_token(vt, nv, err, sizeof(err));
    else if (!str_cmp(key, "quest_completed")
             || !str_cmp(key, "quest_active"))
        result = req_build_quest_ref(key, vt, nv, err, sizeof(err));
    else if (!str_cmp(key, "reputation"))
        result = req_build_reputation(vt, nv, err, sizeof(err));
    else if (!str_cmp(key, "terrain")
             || !str_cmp(key, "sector")
               || !str_cmp(key, "storm")
               || !str_cmp(key, "area_region")
               || !str_cmp(key, "wilds_region"))
        result = req_build_word_or_num_op_key(key, vt, nv, err, sizeof(err));
    else if (!str_cmp(key, "script"))
        result = req_build_script(vt, nv, err, sizeof(err));
    else {
        snprintf(rp->err, sizeof(rp->err),
                 "Unknown requirement key '%s'", key);
        rp->ok = false;
        return NULL;
    }

    if (!result) {
        snprintf(rp->err, sizeof(rp->err), "%s",
                 err[0] ? err : "syntax error");
        rp->ok = false;
    }
    return result;
}

/*
 * rp_parse_primary - '(' expr ')' or atom
 */
static json_t *rp_parse_primary(RParser *rp)
{
    json_t *inner;

    if (!rp->ok) return NULL;

    if (rp->cur.type == RTOK_LPAREN) {
        rp_eat(rp); /* consume '(' */
        inner = rp_parse_expr(rp);
        if (!rp->ok) { json_decref(inner); return NULL; }
        if (rp->cur.type != RTOK_RPAREN) {
            snprintf(rp->err, sizeof(rp->err), "Expected ')'");
            rp->ok = false;
            json_decref(inner);
            return NULL;
        }
        rp_eat(rp); /* consume ')' */
        return inner;
    }

    return rp_parse_atom(rp);
}

/*
 * rp_parse_and_expr - primary ('AND' primary)*
 */
static json_t *rp_parse_and_expr(RParser *rp)
{
    json_t *first;
    json_t *arr;

    if (!rp->ok) return NULL;

    first = rp_parse_primary(rp);
    if (!rp->ok) { json_decref(first); return NULL; }

    if (rp->cur.type != RTOK_AND)
        return first;

    arr = json_array();
    json_array_append_new(arr, first);

    while (rp->ok && rp->cur.type == RTOK_AND) {
        json_t *next;
        rp_eat(rp); /* consume AND */
        next = rp_parse_primary(rp);
        if (!rp->ok) {
            json_decref(arr);
            json_decref(next);
            return NULL;
        }
        json_array_append_new(arr, next);
    }

    {
        json_t *obj = json_object();
        json_object_set_new(obj, "all_of", arr);
        return obj;
    }
}

/*
 * rp_parse_expr - and_expr ('OR' and_expr)*   (top of precedence chain)
 */
static json_t *rp_parse_expr(RParser *rp)
{
    json_t *first;
    json_t *arr;

    if (!rp->ok) return NULL;

    first = rp_parse_and_expr(rp);
    if (!rp->ok) { json_decref(first); return NULL; }

    if (rp->cur.type != RTOK_OR)
        return first;

    arr = json_array();
    json_array_append_new(arr, first);

    while (rp->ok && rp->cur.type == RTOK_OR) {
        json_t *next;
        rp_eat(rp); /* consume OR */
        next = rp_parse_and_expr(rp);
        if (!rp->ok) {
            json_decref(arr);
            json_decref(next);
            return NULL;
        }
        json_array_append_new(arr, next);
    }

    {
        json_t *obj = json_object();
        json_object_set_new(obj, "any_of", arr);
        return obj;
    }
}

/* ---- Public: text -> JSON --------------------------------------- */

/**
 * requirements_text_to_json - compile DSL text to a JSON string
 *
 * Parses builder-friendly DSL text and returns the JSON string
 * expected by requirements_evaluate_text().
 * The caller must free() the returned string.
 * Returns NULL on parse error; err_buf receives a message if non-NULL.
 *
 * @param text        DSL text to compile
 * @param err_buf     buffer to receive error message (may be NULL)
 * @param err_buf_sz  size of err_buf
 * @return            malloc'd JSON string, or NULL on error
 */
char *requirements_text_to_json(const char *text,
                                char *err_buf, size_t err_buf_sz)
{
    RParser     rp;
    json_t     *root;
    char       *json_str;
    const char *player_string = NULL;
    char        expr_buf[MAX_STRING_LENGTH];

    if (IS_NULLSTR(text)) {
        if (err_buf && err_buf_sz)
            snprintf(err_buf, err_buf_sz, "Empty input.");
        return NULL;
    }

    /* Scan for a top-level comma separating the expression from an optional
     * player-visible display string:  "tot_level 50, Level 50 required" */
    {
        const char *p = text;
        int         depth = 0;
        while (*p) {
            if      (*p == '(') depth++;
            else if (*p == ')') depth--;
            else if (*p == ',' && depth == 0) {
                size_t elen = (size_t)(p - text);
                if (elen >= sizeof(expr_buf)) elen = sizeof(expr_buf) - 1;
                strncpy(expr_buf, text, elen);
                /* trim trailing whitespace from expression part */
                while (elen > 0 &&
                       (expr_buf[elen-1] == ' ' || expr_buf[elen-1] == '\t'))
                    elen--;
                expr_buf[elen] = '\0';
                /* player_string starts after comma; skip leading whitespace */
                p++;
                while (*p == ' ' || *p == '\t') p++;
                player_string = p;
                text = expr_buf;
                break;
            }
            p++;
        }
    }

    rp_init(&rp, text);
    root = rp_parse_expr(&rp);

    if (!rp.ok || rp.cur.type != RTOK_EOF) {
        if (err_buf && err_buf_sz) {
            if (!rp.ok)
                snprintf(err_buf, err_buf_sz, "%s", rp.err);
            else
                snprintf(err_buf, err_buf_sz,
                         "Unexpected token '%s' after expression",
                         rp.cur.text);
        }
        json_decref(root);
        return NULL;
    }

    if (!root) {
        if (err_buf && err_buf_sz)
            snprintf(err_buf, err_buf_sz, "Empty expression.");
        return NULL;
    }

    /* Resolve optional player_string and hidden keyword from the comma suffix.
     * Syntax: "expr, My Label" or "expr, hidden" or "expr, My Label, hidden" */
    {
        bool   hidden = false;
        char   ps_buf[MAX_STRING_LENGTH];

        if (player_string && *player_string) {
            strncpy(ps_buf, player_string, sizeof(ps_buf) - 1);
            ps_buf[sizeof(ps_buf) - 1] = '\0';

            /* Trim trailing whitespace */
            {
                size_t plen = strlen(ps_buf);
                while (plen > 0 &&
                       (ps_buf[plen-1] == ' ' || ps_buf[plen-1] == '\t'))
                    plen--;
                ps_buf[plen] = '\0';
            }

            /* Is the whole string just "hidden"? */
            if (!str_cmp(ps_buf, "hidden")) {
                hidden = true;
                player_string = NULL;
            } else {
                /* Check for a trailing ", hidden" segment */
                const char *lc = strrchr(ps_buf, ',');
                if (lc) {
                    const char *after = lc + 1;
                    while (*after == ' ' || *after == '\t') after++;
                    if (!str_cmp(after, "hidden")) {
                        hidden = true;
                        size_t trim = (size_t)(lc - ps_buf);
                        while (trim > 0 &&
                               (ps_buf[trim-1] == ' ' || ps_buf[trim-1] == '\t'))
                            trim--;
                        ps_buf[trim] = '\0';
                        player_string = (trim > 0) ? ps_buf : NULL;
                    } else {
                        player_string = ps_buf;
                    }
                } else {
                    player_string = ps_buf;
                }
            }
        }

        if (player_string && *player_string)
            json_object_set_new(root, "player_string", json_string(player_string));
        if (hidden)
            json_object_set_new(root, "hidden", json_true());
    }

    json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    if (!json_str) {
        if (err_buf && err_buf_sz)
            snprintf(err_buf, err_buf_sz, "JSON serialisation failed.");
        return NULL;
    }

    return json_str; /* caller must free() */
}

/* ---- JSON -> text decompiler ------------------------------------ */

typedef struct {
    char  *buf;
    size_t pos;
    size_t sz;
} RBuf;

static void rbuf_cat(RBuf *b, const char *s)
{
    size_t len = strlen(s);
    if (b->pos + len < b->sz) {
        memcpy(b->buf + b->pos, s, len);
        b->pos += len;
        b->buf[b->pos] = '\0';
    }
}

static void rbuf_cat_long(RBuf *b, long n)
{
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%ld", n);
    rbuf_cat(b, tmp);
}

/* Forward declaration */
static void req_decompile_node(const json_t *node, RBuf *b, bool wrap);

static void req_decompile_array(const json_t *arr, RBuf *b,
                                const char *sep)
{
    size_t  i;
    json_t *item;
    bool    first = true;

    json_array_foreach((json_t *)arr, i, item) {
        if (!first)
            rbuf_cat(b, sep);
        first = false;
        req_decompile_node(item, b, true);
    }
}

static void req_decompile_node(const json_t *node, RBuf *b, bool wrap)
{
    json_t *v;

    if (!node || !json_is_object(node)) {
        rbuf_cat(b, "?");
        return;
    }

    /* all_of */
    v = json_object_get(node, "all_of");
    if (v && json_is_array(v)) {
        if (wrap) rbuf_cat(b, "(");
        req_decompile_array(v, b, " AND ");
        if (wrap) rbuf_cat(b, ")");
        return;
    }

    /* any_of */
    v = json_object_get(node, "any_of");
    if (v && json_is_array(v)) {
        if (wrap) rbuf_cat(b, "(");
        req_decompile_array(v, b, " OR ");
        if (wrap) rbuf_cat(b, ")");
        return;
    }

    /* tot_level */
    v = json_object_get(node, "tot_level");
    if (v) {
        rbuf_cat(b, "tot_level ");
        if (json_is_integer(v)) {
            rbuf_cat_long(b, json_integer_value(v));
        } else if (json_is_object(v)) {
            json_t *ov = json_object_get(v, "op");
            json_t *nv2 = json_object_get(v, "value");
            if (json_is_string(ov)) rbuf_cat(b, json_string_value(ov));
            rbuf_cat(b, " ");
            if (json_is_integer(nv2)) rbuf_cat_long(b, json_integer_value(nv2));
        }
        return;
    }

    /* quest_points */
    v = json_object_get(node, "quest_points");
    if (v) {
        rbuf_cat(b, "quest_points ");
        if (json_is_integer(v)) {
            rbuf_cat_long(b, json_integer_value(v));
        } else if (json_is_object(v)) {
            json_t *ov = json_object_get(v, "op");
            json_t *nv2 = json_object_get(v, "value");
            if (json_is_string(ov)) rbuf_cat(b, json_string_value(ov));
            rbuf_cat(b, " ");
            if (json_is_integer(nv2)) rbuf_cat_long(b, json_integer_value(nv2));
        }
        return;
    }

    /* staff_rank */
    v = json_object_get(node, "staff_rank");
    if (v) {
        rbuf_cat(b, "staff_rank ");
        if (json_is_integer(v)) {
            rbuf_cat_long(b, json_integer_value(v));
        } else if (json_is_string(v)) {
            rbuf_cat(b, json_string_value(v));
        } else if (json_is_object(v)) {
            json_t *ov  = json_object_get(v, "op");
            json_t *val = json_object_get(v, "value");
            if (json_is_string(ov)) rbuf_cat(b, json_string_value(ov));
            rbuf_cat(b, " ");
            if (json_is_integer(val))      rbuf_cat_long(b, json_integer_value(val));
            else if (json_is_string(val))  rbuf_cat(b, json_string_value(val));
        }
        return;
    }

    /* race */
    v = json_object_get(node, "race");
    if (v && json_is_string(v)) {
        rbuf_cat(b, "race ");
        rbuf_cat(b, json_string_value(v));
        return;
    }

    /* class_current */
    v = json_object_get(node, "class_current");
    if (v && json_is_string(v)) {
        rbuf_cat(b, "class_current ");
        rbuf_cat(b, json_string_value(v));
        return;
    }

    /* class_available */
    v = json_object_get(node, "class_available");
    if (v && json_is_string(v)) {
        rbuf_cat(b, "class_available ");
        rbuf_cat(b, json_string_value(v));
        return;
    }

    /* plr_flag */
    v = json_object_get(node, "plr_flag");
    if (v) {
        rbuf_cat(b, "plr_flag ");
        if (json_is_string(v)) {
            rbuf_cat(b, json_string_value(v));
        } else if (json_is_object(v)) {
            json_t *nm  = json_object_get(v, "name");
            json_t *val = json_object_get(v, "value");
            if (json_is_string(nm)) rbuf_cat(b, json_string_value(nm));
            if (json_is_boolean(val) && !json_boolean_value(val))
                rbuf_cat(b, " false");
        }
        return;
    }

    /* class_level */
    v = json_object_get(node, "class_level");
    if (v && json_is_object(v)) {
        json_t *cn = json_object_get(v, "class");
        json_t *lv = json_object_get(v, "level");
        json_t *ov = json_object_get(v, "op");
        rbuf_cat(b, "class_level ");
        if (json_is_string(cn)) rbuf_cat(b, json_string_value(cn));
        rbuf_cat(b, " ");
        if (ov && json_is_string(ov)) {
            rbuf_cat(b, json_string_value(ov));
            rbuf_cat(b, " ");
        }
        if (json_is_integer(lv)) rbuf_cat_long(b, json_integer_value(lv));
        return;
    }

    /* token */
    v = json_object_get(node, "token");
    if (v) {
        rbuf_cat(b, "token ");
        if (json_is_string(v)) {
            rbuf_cat(b, json_string_value(v));
        } else if (json_is_integer(v)) {
            rbuf_cat_long(b, json_integer_value(v));
        } else if (json_is_object(v)) {
            json_t *wn = json_object_get(v, "wnum");
            json_t *mc = json_object_get(v, "min_count");
            if (json_is_string(wn)) rbuf_cat(b, json_string_value(wn));
            if (json_is_integer(mc) && json_integer_value(mc) > 1) {
                rbuf_cat(b, " count ");
                rbuf_cat_long(b, json_integer_value(mc));
            }
        }
        return;
    }

    /* quest_completed */
    v = json_object_get(node, "quest_completed");
    if (v) {
        rbuf_cat(b, "quest_completed ");
        if (json_is_string(v))         rbuf_cat(b, json_string_value(v));
        else if (json_is_integer(v))   rbuf_cat_long(b, json_integer_value(v));
        return;
    }

    /* quest_active */
    v = json_object_get(node, "quest_active");
    if (v) {
        rbuf_cat(b, "quest_active ");
        if (json_is_string(v))         rbuf_cat(b, json_string_value(v));
        else if (json_is_integer(v))   rbuf_cat_long(b, json_integer_value(v));
        return;
    }

    /* reputation */
    v = json_object_get(node, "reputation");
    if (v) {
        rbuf_cat(b, "reputation ");
        if (json_is_string(v)) {
            rbuf_cat(b, json_string_value(v));
        } else if (json_is_object(v)) {
            json_t *wn = json_object_get(v, "wnum");
            json_t *rk = json_object_get(v, "rank");
            json_t *ov = json_object_get(v, "op");
            if (json_is_string(wn)) rbuf_cat(b, json_string_value(wn));
            if (json_is_integer(rk)) {
                rbuf_cat(b, " rank ");
                if (ov && json_is_string(ov)) {
                    rbuf_cat(b, json_string_value(ov));
                    rbuf_cat(b, " ");
                }
                rbuf_cat_long(b, json_integer_value(rk));
            }
        }
        return;
    }

    /* script */
    v = json_object_get(node, "script");
    if (v) {
        rbuf_cat(b, "script");
        if (json_is_string(v)) {
            rbuf_cat(b, " ");
            rbuf_cat(b, json_string_value(v));
        }
        return;
    }

    /* terrain */
    v = json_object_get(node, "terrain");
    if (v) {
        rbuf_cat(b, "terrain ");
        if (json_is_string(v))
            rbuf_cat(b, json_string_value(v));
        else if (json_is_integer(v))
            rbuf_cat_long(b, json_integer_value(v));
        else if (json_is_object(v)) {
            json_t *ov = json_object_get(v, "op");
            json_t *vv = json_object_get(v, "value");
            if (ov && json_is_string(ov)) {
                rbuf_cat(b, json_string_value(ov));
                rbuf_cat(b, " ");
            }
            if (json_is_string(vv))
                rbuf_cat(b, json_string_value(vv));
            else if (json_is_integer(vv))
                rbuf_cat_long(b, json_integer_value(vv));
        }
        return;
    }

    /* sector */
    v = json_object_get(node, "sector");
    if (v) {
        rbuf_cat(b, "sector ");
        if (json_is_string(v))
            rbuf_cat(b, json_string_value(v));
        else if (json_is_integer(v))
            rbuf_cat_long(b, json_integer_value(v));
        else if (json_is_object(v)) {
            json_t *ov = json_object_get(v, "op");
            json_t *vv = json_object_get(v, "value");
            if (ov && json_is_string(ov)) {
                rbuf_cat(b, json_string_value(ov));
                rbuf_cat(b, " ");
            }
            if (json_is_string(vv))
                rbuf_cat(b, json_string_value(vv));
            else if (json_is_integer(vv))
                rbuf_cat_long(b, json_integer_value(vv));
        }
        return;
    }

    /* storm */
    v = json_object_get(node, "storm");
    if (v) {
        rbuf_cat(b, "storm ");
        if (json_is_boolean(v))
            rbuf_cat(b, json_boolean_value(v) ? "any" : "none");
        else if (json_is_string(v))
            rbuf_cat(b, json_string_value(v));
        else if (json_is_integer(v))
            rbuf_cat_long(b, json_integer_value(v));
        else if (json_is_object(v)) {
            json_t *active = json_object_get(v, "active");
            json_t *ov = json_object_get(v, "op");
            json_t *vv = json_object_get(v, "value");
            json_t *tv = json_object_get(v, "type");

            if (json_is_boolean(active)) {
                rbuf_cat(b, json_boolean_value(active) ? "any" : "none");
                return;
            }

            if (!tv)
                tv = vv;

            if (ov && json_is_string(ov)) {
                rbuf_cat(b, json_string_value(ov));
                rbuf_cat(b, " ");
            }

            if (json_is_string(tv))
                rbuf_cat(b, json_string_value(tv));
            else if (json_is_integer(tv))
                rbuf_cat_long(b, json_integer_value(tv));
        }
        return;
    }

    /* area_region */
    v = json_object_get(node, "area_region");
    if (v) {
        rbuf_cat(b, "area_region ");
        if (json_is_string(v))
            rbuf_cat(b, json_string_value(v));
        else if (json_is_integer(v))
            rbuf_cat_long(b, json_integer_value(v));
        else if (json_is_object(v)) {
            json_t *ov = json_object_get(v, "op");
            json_t *vv = json_object_get(v, "value");
            json_t *uv = json_object_get(v, "uid");
            json_t *nv = json_object_get(v, "name");

            if (ov && json_is_string(ov)) {
                rbuf_cat(b, json_string_value(ov));
                rbuf_cat(b, " ");
            }

            if (!vv)
                vv = uv ? uv : nv;

            if (json_is_string(vv))
                rbuf_cat(b, json_string_value(vv));
            else if (json_is_integer(vv))
                rbuf_cat_long(b, json_integer_value(vv));
        }
        return;
    }

    /* wilds_region */
    v = json_object_get(node, "wilds_region");
    if (v) {
        rbuf_cat(b, "wilds_region ");
        if (json_is_string(v))
            rbuf_cat(b, json_string_value(v));
        else if (json_is_integer(v))
            rbuf_cat_long(b, json_integer_value(v));
        else if (json_is_object(v)) {
            json_t *ov = json_object_get(v, "op");
            json_t *vv = json_object_get(v, "value");
            if (ov && json_is_string(ov)) {
                rbuf_cat(b, json_string_value(ov));
                rbuf_cat(b, " ");
            }
            if (json_is_string(vv))
                rbuf_cat(b, json_string_value(vv));
            else if (json_is_integer(vv))
                rbuf_cat_long(b, json_integer_value(vv));
        }
        return;
    }

    rbuf_cat(b, "{?unknown?}");
}

/**
 * requirements_json_to_text - decompile a stored JSON prerequisites string
 *
 * Converts the internally-stored JSON back to builder-friendly DSL text.
 * The caller must free() the returned string.
 * Returns NULL if json_str is NULL or empty.
 *
 * @param json_str  JSON string stored in prerequisites field
 * @return          malloc'd DSL text string, or NULL
 */
char *requirements_json_to_text(const char *json_str)
{
    json_t      *root;
    json_error_t jerr;
    char         buf[MAX_STRING_LENGTH];
    RBuf         b;

    if (IS_NULLSTR(json_str))
        return NULL;

    root = json_loads(json_str, 0, &jerr);
    if (!root)
        return strdup(json_str); /* fall back: return raw on parse failure */

    b.buf = buf;
    b.pos = 0;
    b.sz  = sizeof(buf) - 1;
    buf[0] = '\0';

    req_decompile_node(root, &b, false);

    /* Append optional player-visible display string and hidden flag as comma suffixes */
    {
        json_t *ps = json_object_get(root, "player_string");
        json_t *hf = json_object_get(root, "hidden");
        if (ps && json_is_string(ps)) {
            rbuf_cat(&b, ", ");
            rbuf_cat(&b, json_string_value(ps));
        }
        if (hf && json_is_true(hf))
            rbuf_cat(&b, ", hidden");
    }

    json_decref(root);

    return strdup(buf);
}

/**
 * requirements_is_hidden - check if prerequisites have the hidden flag set
 *
 * When true, an unmet requirement with no player_string will show a generic
 * "additional requirements" hint rather than being completely silent.
 *
 * @param json_str  prerequisites JSON string
 * @return          true if "hidden": true is stored
 */
bool requirements_is_hidden(const char *json_str)
{
    json_t      *root;
    json_error_t jerr;
    json_t      *h;
    bool         result;

    if (IS_NULLSTR(json_str))
        return false;

    root = json_loads(json_str, 0, &jerr);
    if (!root)
        return false;

    h      = json_object_get(root, "hidden");
    result = (h && json_is_true(h));
    json_decref(root);
    return result;
}

/**
 * requirements_get_player_string - extract the optional display string
 *
 * Returns a malloc'd copy of the "player_string" field stored in the
 * prerequisites JSON, or NULL if absent.  Caller must free() the result.
 *
 * @param json_str  prerequisites JSON string
 * @return          malloc'd display string, or NULL
 */
char *requirements_get_player_string(const char *json_str)
{
    json_t      *root;
    json_error_t jerr;
    json_t      *ps;
    char        *result;

    if (IS_NULLSTR(json_str))
        return NULL;

    root = json_loads(json_str, 0, &jerr);
    if (!root)
        return NULL;

    ps     = json_object_get(root, "player_string");
    result = (ps && json_is_string(ps)) ? strdup(json_string_value(ps)) : NULL;
    json_decref(root);
    return result;
}

bool requirements_evaluate_json(const json_t *spec,
                                const REQUIREMENT_CONTEXT *context,
                                bool default_if_empty)
{
    if (!spec)
        return default_if_empty;

    return requirements_eval_node(spec, context, 0);
}

bool requirements_evaluate_text(const char *spec_json,
                                const REQUIREMENT_CONTEXT *context,
                                bool default_if_empty)
{
    json_t *root;
    json_error_t err;
    bool result;

    if (IS_NULLSTR(spec_json))
        return default_if_empty;

    root = json_loads(spec_json, 0, &err);
    if (!root) {
        log_stringf("Requirements: parse error at line %d: %s", err.line, err.text);
        return false;
    }

    result = requirements_evaluate_json(root, context, default_if_empty);
    json_decref(root);
    return result;
}
#include <stdlib.h>
#include <string.h>
#include <jansson.h>

#include "merc.h"
#include "tables.h"
#include "requirements.h"

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
        flag_name = json_string_value(json_object_get(value, "name"));
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
        const char *op_name = json_string_value(json_object_get(value, "op"));
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
        const char *op_name = json_string_value(json_object_get(value, "op"));
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
            !str_cmp(key, "token")) {
            has_known = true;
            result = result && requirements_eval_leaf(key, value, context);
            continue;
        }

        return false;
    }

    if (!has_known)
        return false;

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
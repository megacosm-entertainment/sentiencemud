/**
 * @file olc_staged.c
 * @brief Preview helpers and staging utilities — implementation.
 */

#include "olc_staged.h"
#include <string.h>

const char *olc_staged_string(olc_changeset_t *cs, const char *field, const char *live)
{
    if (!cs || !field) return live;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    if (!change || !change->new_value) return live;

    const char *val = json_string_value(change->new_value);
    return val ? val : live;
}

int olc_staged_int(olc_changeset_t *cs, const char *field, int live)
{
    if (!cs || !field) return live;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    if (!change || !change->new_value || !json_is_integer(change->new_value))
        return live;

    return (int)json_integer_value(change->new_value);
}

long olc_staged_flags(olc_changeset_t *cs, const char *field, long live)
{
    if (!cs || !field) return live;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    if (!change || !change->new_value || !json_is_integer(change->new_value))
        return live;

    return (long)json_integer_value(change->new_value);
}

bool olc_staged_bool(olc_changeset_t *cs, const char *field, bool live)
{
    if (!cs || !field) return live;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    if (!change || !change->new_value) return live;

    if (json_is_true(change->new_value)) return true;
    if (json_is_false(change->new_value)) return false;
    return live;
}

json_t *olc_staged_json(olc_changeset_t *cs, const char *field)
{
    if (!cs || !field) return NULL;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    return change ? change->new_value : NULL;
}

bool olc_is_field_staged(olc_changeset_t *cs, const char *field)
{
    return olc_changeset_find_change(cs, field) != NULL;
}

const char *olc_staged_marker(olc_changeset_t *cs, const char *field)
{
    return olc_is_field_staged(cs, field) ? "{Y*{x " : "";
}

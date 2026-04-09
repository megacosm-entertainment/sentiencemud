/**
 * @file olc_actions.h
 * @brief Action handler infrastructure for OLC editors (Phase 5)
 *
 * Provides types, a handler registry, action session tracking, and
 * GMCP message builders for add/remove action operations.
 */

#ifndef __OLC_ACTIONS_H__
#define __OLC_ACTIONS_H__

#include "olc_changeset.h"
#include <jansson.h>

typedef struct char_data CHAR_DATA;

/** Callback: build the form JSON describing available fields for an action. */
typedef json_t *(*olc_action_form_fn)(void *entity, CHAR_DATA *ch);

/** Callback: stage the action values into a changeset. */
typedef bool (*olc_action_stage_fn)(void *entity, olc_changeset_t *cs,
                                     json_t *values,
                                     char *errbuf, size_t errlen);

/**
 * Describes a single action handler (e.g. "add_affect", "remove_affect").
 */
typedef struct olc_action_handler {
    const char          *action_name;
    int                  editor_type;
    const char          *display_hint;
    const char          *title;
    const char          *list_name;
    olc_action_form_fn   form_fn;
    olc_action_stage_fn  stage_fn;
} olc_action_handler_t;

/* olc_action_session_t is defined in olc_changeset.h */

/* Registry */
void olc_register_actions(const olc_action_handler_t *table);
const olc_action_handler_t *olc_find_action(int editor_type, const char *action_name);

/* Session management */
olc_action_session_t *olc_action_session_create(olc_edit_state_t *state,
    int editor_type, const char *action_name);
olc_action_session_t *olc_action_session_find(olc_edit_state_t *state,
    const char *session_id);
void olc_action_session_clear(olc_edit_state_t *state);

/* GMCP message builders */
json_t *olc_action_build_form(const char *entity_id,
    const olc_action_handler_t *handler,
    const olc_action_session_t *session,
    json_t *fields);
json_t *olc_action_build_result(const char *entity_id,
    const char *session_id,
    bool success, const char *message);

#endif /* __OLC_ACTIONS_H__ */

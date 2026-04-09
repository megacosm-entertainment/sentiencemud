/**
 * @file olc_actions.c
 * @brief Implementation of OLC action handler registry, session tracking,
 *        and GMCP message builders (Phase 5)
 */

#include <string.h>
#include <time.h>

#include "../../merc.h"
#include "olc_actions.h"

/* =========================================================================
 * Action Handler Registry
 * ========================================================================= */

#define MAX_ACTION_TABLES 8

static const olc_action_handler_t *action_tables[MAX_ACTION_TABLES];
static int action_table_count = 0;

/**
 * Register a NULL-terminated array of action handlers.
 */
void olc_register_actions(const olc_action_handler_t *table)
{
    if (!table)
        return;

    if (action_table_count >= MAX_ACTION_TABLES) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                    "olc_register_actions: table limit (%d) reached",
                    MAX_ACTION_TABLES);
        return;
    }

    action_tables[action_table_count++] = table;
}

/**
 * Find a handler matching editor_type + action_name (case-insensitive).
 */
const olc_action_handler_t *olc_find_action(int editor_type, const char *action_name)
{
    if (!action_name)
        return NULL;

    for (int t = 0; t < action_table_count; t++) {
        const olc_action_handler_t *tbl = action_tables[t];

        for (int i = 0; tbl[i].action_name != NULL; i++) {
            if (tbl[i].editor_type == editor_type
                && !str_cmp(tbl[i].action_name, action_name)) {
                return &tbl[i];
            }
        }
    }

    return NULL;
}

/* =========================================================================
 * Action Session Management
 * ========================================================================= */

/**
 * Create a new action session, replacing any existing one.
 * Returns pointer to the embedded session struct.
 */
olc_action_session_t *olc_action_session_create(olc_edit_state_t *state,
    int editor_type, const char *action_name)
{
    if (!state || !action_name)
        return NULL;

    /* Clear any existing session */
    olc_action_session_clear(state);

    olc_action_session_t *session = &state->action_session;

    snprintf(session->session_id, sizeof(session->session_id),
             "act_%d", state->next_action_session_id++);
    strlcpy(session->action_name, action_name, sizeof(session->action_name));
    session->editor_type = editor_type;
    session->created_at  = time(NULL);

    state->has_action_session = true;

    return session;
}

/**
 * Find the active action session by session_id.
 * Returns NULL if no session is active or ID doesn't match.
 */
olc_action_session_t *olc_action_session_find(olc_edit_state_t *state,
    const char *session_id)
{
    if (!state || !session_id || !state->has_action_session)
        return NULL;

    if (strcmp(state->action_session.session_id, session_id) == 0)
        return &state->action_session;

    return NULL;
}

/**
 * Clear the active action session.
 */
void olc_action_session_clear(olc_edit_state_t *state)
{
    if (!state)
        return;

    memset(&state->action_session, 0, sizeof(state->action_session));
    state->has_action_session = false;
}

/* =========================================================================
 * GMCP Message Builders
 * ========================================================================= */

/**
 * Build an action form JSON message.
 * Uses borrowed reference (O) for fields — caller owns the fields array.
 */
json_t *olc_action_build_form(const char *entity_id,
    const olc_action_handler_t *handler,
    const olc_action_session_t *session,
    json_t *fields)
{
    if (!entity_id || !handler || !session || !fields)
        return NULL;

    return json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:O}",
        "entity_id",    entity_id,
        "action",       handler->action_name,
        "session_id",   session->session_id,
        "display_hint", handler->display_hint ? handler->display_hint : "",
        "title",        handler->title ? handler->title : "",
        "list_name",    handler->list_name ? handler->list_name : "",
        "fields",       fields);
}

/**
 * Build an action result JSON message.
 * Adds "message" field only when success is false.
 */
json_t *olc_action_build_result(const char *entity_id,
    const char *session_id,
    bool success, const char *message)
{
    if (!entity_id || !session_id)
        return NULL;

    json_t *result = json_pack("{s:s, s:s, s:s}",
        "entity_id",  entity_id,
        "session_id", session_id,
        "status",     success ? "success" : "error");

    if (!success && message) {
        json_object_set_new(result, "message", json_string(message));
    }

    return result;
}

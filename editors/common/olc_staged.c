/**
 * @file olc_staged.c
 * @brief Preview helpers and staging utilities — implementation.
 */

#include "olc_staged.h"
#include "olc_editor.h"
#include "olc_field_handlers.h"
#include "../../gmcp_editor.h"
#include "olc_commit_history.h"
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

/* =========================================================================
 * Active Changeset Lookup
 * ========================================================================= */

#include "olc_editor.h"
#include "olc_field_handlers.h"

olc_changeset_t *olc_get_active_changeset(CHAR_DATA *ch,
    const OLC_EDITOR_DEF *def)
{
    if (!ch || !ch->desc || !def) return NULL;
    if (def->change_mode != OLC_CHANGE_STAGED) return NULL;
    if (!ch->desc->olc_state) return NULL;

    WNUM_LOAD wnum = { 0, 0 };

    void *pEdit = ch->desc->pEdit;
    if (!pEdit) return NULL;

    switch (def->editor_type) {
        case ED_ROOM: {
            ROOM_INDEX_DATA *r = (ROOM_INDEX_DATA *)pEdit;
            wnum.auid = r->area->uid;
            wnum.vnum = r->vnum;
            break;
        }
        case ED_MOBILE: {
            MOB_INDEX_DATA *m = (MOB_INDEX_DATA *)pEdit;
            wnum.auid = m->area->uid;
            wnum.vnum = m->vnum;
            break;
        }
        case ED_OBJECT: {
            OBJ_INDEX_DATA *o = (OBJ_INDEX_DATA *)pEdit;
            wnum.auid = o->area->uid;
            wnum.vnum = o->vnum;
            break;
        }
        case ED_AREA: {
            AREA_DATA *a = (AREA_DATA *)pEdit;
            wnum.auid = a->uid;
            wnum.vnum = 0;
            break;
        }
        default:
            return NULL;
    }

    return olc_edit_state_find_changeset(ch->desc->olc_state,
        def->editor_type, wnum);
}

bool olc_check_staging_limits(CHAR_DATA *ch, olc_changeset_t *cs)
{
    if (!ch || !cs) return false;

    if (olc_changeset_count(cs) >= OLC_MAX_PENDING_PER_ENTITY) {
        send_to_char("{RLimit:{x Too many pending changes. Commit or revert first.\n\r", ch);
        return false;
    }

    if (ch->desc && ch->desc->olc_state
        && olc_edit_state_total_pending(ch->desc->olc_state)
            >= OLC_MAX_PENDING_PER_BUILDER) {
        send_to_char("{RLimit:{x Builder-wide change limit reached. Commit or revert.\n\r", ch);
        return false;
    }

    return true;
}

/* =========================================================================
 * Staged Editor Commands
 * ========================================================================= */

static const char *field_type_label(olc_field_type_t type)
{
    switch (type) {
        case OLC_FIELD_STRING:  return "string";
        case OLC_FIELD_INT:     return "int";
        case OLC_FIELD_INT16:   return "int16";
        case OLC_FIELD_BOOL:    return "bool";
        case OLC_FIELD_FLAGS:   return "flags";
        case OLC_FIELD_MULTIFLAGS: return "mflags";
        case OLC_FIELD_WIDEVNUM: return "wnum";
        case OLC_FIELD_EXIT:    return "exit";
        case OLC_FIELD_EMBEDDED: return "embed";
        case OLC_FIELD_LIST_ADD:    return "list+";
        case OLC_FIELD_LIST_REMOVE: return "list-";
        case OLC_FIELD_LIST_UPDATE: return "list~";
        case OLC_FIELD_MULTILINE:   return "text";
        case OLC_FIELD_TYPE_DATA:   return "type";
        default: return "?";
    }
}

static void format_json_brief(char *buf, size_t bufsz, json_t *val)
{
    if (!val) {
        snprintf(buf, bufsz, "(null)");
        return;
    }

    if (json_is_string(val)) {
        const char *s = json_string_value(val);
        if (strlen(s) > 40)
            snprintf(buf, bufsz, "\"%.37s...\"", s);
        else
            snprintf(buf, bufsz, "\"%s\"", s);
    } else if (json_is_integer(val)) {
        snprintf(buf, bufsz, "%lld", (long long)json_integer_value(val));
    } else if (json_is_true(val)) {
        snprintf(buf, bufsz, "true");
    } else if (json_is_false(val)) {
        snprintf(buf, bufsz, "false");
    } else {
        snprintf(buf, bufsz, "(complex)");
    }
}

void olc_staged_cmd_pending(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit)
{
    olc_changeset_t *cs = olc_get_active_changeset(ch, def);
    if (!cs || olc_changeset_count(cs) == 0) {
        send_to_char("No pending changes.\n\r", ch);
        return;
    }

    printf_to_char(ch, "{WPending changes for %s:{x\n\r", cs->entity_label);
    printf_to_char(ch, "{D%-25s %-8s %-20s %-20s{x\n\r",
        "Field", "Type", "Old", "New");
    printf_to_char(ch, "{D%.73s{x\n\r",
        "-------------------------------------------------------------------------");

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = (olc_pending_change_t *)iterator_nextdata(&it)) != NULL) {
        char old_buf[64], new_buf[64];
        format_json_brief(old_buf, sizeof(old_buf), change->old_value);
        format_json_brief(new_buf, sizeof(new_buf), change->new_value);

        printf_to_char(ch, " {Y%-24s{x %-8s %-20s {W%-20s{x\n\r",
            change->field_path,
            field_type_label(change->field_type),
            old_buf, new_buf);
    }
    iterator_stop(&it);

    printf_to_char(ch, "\n\r{x%d pending change%s.\n\r",
        olc_changeset_count(cs),
        olc_changeset_count(cs) == 1 ? "" : "s");
}

void olc_staged_cmd_revert(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit, char *argument)
{
    olc_changeset_t *cs = olc_get_active_changeset(ch, def);
    if (!cs) {
        send_to_char("No active changeset.\n\r", ch);
        return;
    }

    if (IS_NULLSTR(argument)) {
        int count = olc_changeset_count(cs);
        if (count == 0) {
            send_to_char("No pending changes to revert.\n\r", ch);
            return;
        }
        olc_changeset_revert(cs);
        printf_to_char(ch, "{G[REVERTED]{x All %d pending change%s discarded.\n\r",
            count, count == 1 ? "" : "s");
    } else {
        if (olc_changeset_revert_field(cs, argument)) {
            printf_to_char(ch, "{G[REVERTED]{x Change to '%s' discarded.\n\r", argument);
        } else {
            printf_to_char(ch, "No pending change for '%s'.\n\r", argument);
            return;
        }
    }

    /* Notify web client via GMCP */
    if (ch->desc) {
        const char *eid = gmcp_editor_entity_id(
            cs->editor_type, cs->entity_wnum);
        gmcp_editor_send_state(ch->desc, eid, cs, false);
    }
}

static void archive_changeset_to_history(olc_changeset_t *cs, int group_id, const char *comment)
{
    if (!cs || olc_changeset_count(cs) == 0) return;

    olc_commit_history_t *history = olc_commit_history_get_or_load(
        cs->editor_type, cs->entity_wnum);
    if (!history) return;

    olc_commit_history_archive(history, cs, group_id, comment);
}

void olc_staged_cmd_commit(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit, char *argument)
{
    olc_changeset_t *cs = olc_get_active_changeset(ch, def);
    if (!cs) {
        send_to_char("No active changeset.\n\r", ch);
        return;
    }

    if (olc_changeset_count(cs) == 0) {
        send_to_char("No pending changes to commit.\n\r", ch);
        return;
    }

    /* Archive to history BEFORE commit (commit clears the changeset) */
    archive_changeset_to_history(cs, 0, argument);

    const char *error_field = NULL;
    int applied = olc_changeset_commit(cs, pEdit,
        def->field_handlers, &error_field);

    if (applied < 0) {
        printf_to_char(ch, "{RCommit failed:{x Error applying field '%s'.\n\r",
            error_field ? error_field : "unknown");
        return;
    }

    /* Mark entity as changed */
    if (def->get_area_fn && pEdit) {
        AREA_DATA *area = def->get_area_fn(pEdit);
        if (area) {
            SET_BIT(area->area_flags, AREA_CHANGED);
        }
    }

    /* Keep inheritance live */
    if (def->editor_type == ED_ROOM
        || def->editor_type == ED_MOBILE
        || def->editor_type == ED_OBJECT) {
        fix_index_inheritance();
    }

    printf_to_char(ch, "{G[COMMITTED]{x %d change%s applied%s%s.\n\r",
        applied, applied == 1 ? "" : "s",
        IS_NULLSTR(argument) ? "" : ": ",
        IS_NULLSTR(argument) ? "" : argument);

    /* Notify web client via GMCP */
    if (ch->desc) {
        const char *eid = gmcp_editor_entity_id(
            cs->editor_type, cs->entity_wnum);
        gmcp_editor_send_commit_result(ch->desc, eid, "success", applied);
    }
}

void olc_staged_cmd_commit_group(CHAR_DATA *ch, char *argument)
{
    if (!ch || !ch->desc || !ch->desc->olc_state) {
        send_to_char("No active editing state.\n\r", ch);
        return;
    }

    int total_applied = 0;
    int changesets_committed = 0;

    int group_id = olc_next_group_id++;

    ITERATOR it;
    iterator_start(&it, ch->desc->olc_state->active_changesets);
    olc_changeset_t *cs;
    while ((cs = (olc_changeset_t *)iterator_nextdata(&it)) != NULL) {
        if (olc_changeset_count(cs) == 0)
            continue;

        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(cs->editor_type);
        if (!edef) continue;

        /* We need the entity pointer to commit — skip if not the current edit */
        if (ch->desc->editor != cs->editor_type)
            continue;

        archive_changeset_to_history(cs, group_id, argument);

        const char *error_field = NULL;
        int applied = olc_changeset_commit(cs, ch->desc->pEdit,
            edef->field_handlers, &error_field);

        if (applied < 0) {
            printf_to_char(ch, "{RGroup commit failed:{x Error on %s field '%s'.\n\r",
                cs->entity_label, error_field ? error_field : "unknown");
            iterator_stop(&it);
            return;
        }

        if (edef->get_area_fn && ch->desc->pEdit) {
            AREA_DATA *area = edef->get_area_fn(ch->desc->pEdit);
            if (area) SET_BIT(area->area_flags, AREA_CHANGED);
        }

        total_applied += applied;
        changesets_committed++;
    }
    iterator_stop(&it);

    if (changesets_committed == 0) {
        send_to_char("No pending changes across any editors.\n\r", ch);
        return;
    }

    printf_to_char(ch, "{G[GROUP COMMITTED]{x %d change%s across %d changeset%s%s%s.\n\r",
        total_applied, total_applied == 1 ? "" : "s",
        changesets_committed, changesets_committed == 1 ? "" : "s",
        IS_NULLSTR(argument) ? "" : ": ",
        IS_NULLSTR(argument) ? "" : argument);
}

/* =========================================================================
 * Draft Commands
 * ========================================================================= */

void olc_staged_cmd_savedraft(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit)
{
    olc_changeset_t *cs = olc_get_active_changeset(ch, def);
    if (!cs) return;

    if (olc_changeset_count(cs) == 0) {
        send_to_char("No pending changes to save.\n\r", ch);
        return;
    }

    if (olc_draft_save(cs))
        printf_to_char(ch, "{G[DRAFT SAVED]{x %d change%s saved.\n\r",
            olc_changeset_count(cs),
            olc_changeset_count(cs) == 1 ? "" : "s");
    else
        send_to_char("{R[ERROR]{x Failed to save draft.\n\r", ch);
}

void olc_staged_cmd_loaddraft(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit)
{
    if (!ch || !ch->desc || !def || !pEdit) return;

    WNUM_LOAD wnum = olc_get_entity_wnum(def, pEdit);

    if (!olc_draft_exists(ch->name, def->editor_type, wnum)) {
        send_to_char("No saved draft found for this entity.\n\r", ch);
        return;
    }

    olc_changeset_t *loaded = olc_draft_load(ch->name, def->editor_type, wnum);
    if (!loaded) {
        send_to_char("{R[ERROR]{x Failed to load draft (may be corrupt).\n\r", ch);
        return;
    }

    /* Replace the current changeset */
    if (!ch->desc->olc_state)
        ch->desc->olc_state = olc_edit_state_create();

    olc_changeset_t *existing = olc_edit_state_find_changeset(
        ch->desc->olc_state, def->editor_type, wnum);
    if (existing) {
        list_remlink(ch->desc->olc_state->active_changesets, existing, false);
        olc_changeset_destroy(existing);
    }
    list_addlink(ch->desc->olc_state->active_changesets, loaded);

    printf_to_char(ch, "{G[DRAFT LOADED]{x %d change%s restored.\n\r",
        olc_changeset_count(loaded),
        olc_changeset_count(loaded) == 1 ? "" : "s");
}

void olc_staged_cmd_discarddraft(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit)
{
    if (!ch || !ch->desc || !def || !pEdit) return;

    WNUM_LOAD wnum = olc_get_entity_wnum(def, pEdit);

    if (!olc_draft_exists(ch->name, def->editor_type, wnum)) {
        send_to_char("No saved draft found for this entity.\n\r", ch);
        return;
    }

    if (olc_draft_discard(ch->name, def->editor_type, wnum))
        send_to_char("{G[DRAFT DISCARDED]{x Saved draft removed.\n\r", ch);
    else
        send_to_char("{R[ERROR]{x Failed to discard draft.\n\r", ch);
}

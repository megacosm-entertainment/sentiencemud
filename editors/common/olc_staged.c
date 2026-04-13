/**
 * @file olc_staged.c
 * @brief Preview helpers and staging utilities — implementation.
 */

#include "olc_staged.h"
#include "olc_editor.h"
#include "olc_field_handlers.h"
#include "../../gmcp_editor.h"
#include "olc_commit_history.h"
#include "../../utils/buffer.h"
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

long olc_staged_long(olc_changeset_t *cs, const char *field, long live)
{
    if (!cs || !field) return live;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    if (!change || !change->new_value || !json_is_integer(change->new_value))
        return live;

    return (long)json_integer_value(change->new_value);
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

/**
 * Display grouped summary for list operations under a given prefix.
 */
static void display_list_group(CHAR_DATA *ch, olc_changeset_t *cs,
    const char *prefix)
{
    int add_count = 0, rm_count = 0;
    char add_prefix[MIL + 8], rm_prefix[MIL + 8];
    snprintf(add_prefix, sizeof(add_prefix), "%s/add:", prefix);
    snprintf(rm_prefix, sizeof(rm_prefix), "%s/rm:", prefix);
    size_t add_len = strlen(add_prefix);
    size_t rm_len = strlen(rm_prefix);

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        if (strncmp(change->field_path, add_prefix, add_len) == 0)
            add_count++;
        else if (strncmp(change->field_path, rm_prefix, rm_len) == 0)
            rm_count++;
    }
    iterator_stop(&it);

    if (add_count > 0)
        printf_to_char(ch, " {Y%-24s{x %-8s %-20s {G+%d added{x\n\r",
            prefix, "list", "", add_count);
    if (rm_count > 0)
        printf_to_char(ch, " {Y%-24s{x %-8s %-20s {R-%d removed{x\n\r",
            prefix, "list", "", rm_count);
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

    /* Track displayed list prefixes to avoid duplicates */
    char seen_prefixes[20][MIL];
    int seen_count = 0;

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = (olc_pending_change_t *)iterator_nextdata(&it)) != NULL) {
        /* Check if this is a list operation */
        const char *add_sep = strstr(change->field_path, "/add:");
        const char *rm_sep = strstr(change->field_path, "/rm:");
        if (add_sep || rm_sep) {
            /* Extract prefix */
            char prefix[MIL];
            const char *sep = add_sep ? add_sep : rm_sep;
            size_t plen = (size_t)(sep - change->field_path);
            if (plen >= sizeof(prefix)) plen = sizeof(prefix) - 1;
            memcpy(prefix, change->field_path, plen);
            prefix[plen] = '\0';

            /* Check if already displayed */
            bool already_seen = false;
            for (int i = 0; i < seen_count; i++) {
                if (strcmp(seen_prefixes[i], prefix) == 0) {
                    already_seen = true;
                    break;
                }
            }
            if (!already_seen && seen_count < 20) {
                strlcpy(seen_prefixes[seen_count], prefix, MIL);
                seen_count++;
                display_list_group(ch, cs, prefix);
            }
            continue;
        }

        /* Non-list: display as before */
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
            int prefix_removed = olc_changeset_revert_prefix(cs, argument);
            if (prefix_removed > 0) {
                printf_to_char(ch, "{G[REVERTED]{x %d change%s to '%s' discarded.\n\r",
                    prefix_removed, prefix_removed == 1 ? "" : "s", argument);
            } else {
                printf_to_char(ch, "No pending change for '%s'.\n\r", argument);
                return;
            }
        }
    }

    /* Notify web client via GMCP */
    if (ch->desc) {
        const char *eid = gmcp_editor_entity_id(
            cs->editor_type, cs->entity_wnum);
        gmcp_editor_send_state(ch->desc, eid, cs, false);
    }
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

    /* Archive to history BEFORE commit (commit clears the changeset).
     * If commit fails, roll back the archived record. */
    olc_commit_history_t *history = olc_commit_history_get_or_load(
        cs->editor_type, cs->entity_wnum);
    olc_commit_record_t *archived = NULL;
    if (history && olc_changeset_count(cs) > 0)
        archived = olc_commit_history_archive(history, cs, 0, argument);

    const char *error_field = NULL;
    int applied = olc_changeset_commit(cs, pEdit,
        def->field_handlers, &error_field);

    if (applied < 0) {
        /* Roll back the archived record on commit failure */
        if (archived && history) {
            list_remlink(history->records, archived, false);
            olc_commit_record_destroy(archived);
            history->next_id--;
        }
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

        olc_commit_history_t *cs_history = olc_commit_history_get_or_load(
            cs->editor_type, cs->entity_wnum);
        olc_commit_record_t *cs_archived = NULL;
        if (cs_history && olc_changeset_count(cs) > 0)
            cs_archived = olc_commit_history_archive(cs_history, cs, group_id, argument);

        const char *error_field = NULL;
        int applied = olc_changeset_commit(cs, ch->desc->pEdit,
            edef->field_handlers, &error_field);

        if (applied < 0) {
            /* Roll back the archived record on commit failure */
            if (cs_archived && cs_history) {
                list_remlink(cs_history->records, cs_archived, false);
                olc_commit_record_destroy(cs_archived);
                cs_history->next_id--;
            }
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
 * History Commands
 * ========================================================================= */

static void show_history_list(CHAR_DATA *ch, olc_commit_history_t *history,
    const char *argument)
{
    int limit = olc_commit_history_count(history);
    if (!IS_NULLSTR(argument) && is_number(argument))
        limit = UMIN(atoi(argument), limit);

    if (limit == 0) {
        send_to_char("No commit history for this entity.\n\r", ch);
        return;
    }

    BUFFER *buffer = new_buf();
    char buf[MSL];

    snprintf(buf, sizeof(buf),
        "{Y+------+--------------------+-------+-------------------------------+{x\n\r");
    add_buf(buffer, buf);
    snprintf(buf, sizeof(buf),
        "{Y| {WID{x   | {WAuthor{x             | {W# Chg{x | {WDate & Time{x                  |{x\n\r");
    add_buf(buffer, buf);
    snprintf(buf, sizeof(buf),
        "{Y+------+--------------------+-------+-------------------------------+{x\n\r");
    add_buf(buffer, buf);

    int shown = 0;
    ITERATOR it;
    iterator_start(&it, history->records);
    olc_commit_record_t *record;
    while ((record = iterator_nextdata(&it)) != NULL && shown < limit) {
        char time_buf[64];
        struct tm *tm_info = localtime(&record->timestamp);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

        snprintf(buf, sizeof(buf),
            "{Y| {W%-4d{x | %-18.18s | {W%-5d{x | %-29s |{x%s\n\r",
            record->id, record->author,
            list_size(record->changes), time_buf,
            record->group_id > 0 ? " {D[group]{x" : "");
        add_buf(buffer, buf);
        shown++;
    }
    iterator_stop(&it);

    snprintf(buf, sizeof(buf),
        "{Y+------+--------------------+-------+-------------------------------+{x\n\r");
    add_buf(buffer, buf);

    snprintf(buf, sizeof(buf),
        "\n\rUse '{Whistory <id>{x' for details. '{Whistory revert <id>{x' to undo.\n\r");
    add_buf(buffer, buf);

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}

static void show_history_detail(CHAR_DATA *ch, olc_commit_history_t *history,
    int id)
{
    olc_commit_record_t *record = olc_commit_history_find(history, id);
    if (!record) {
        printf_to_char(ch, "No commit record with ID %d.\n\r", id);
        return;
    }

    char time_buf[64];
    struct tm *tm_info = localtime(&record->timestamp);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    printf_to_char(ch, "{WCommit #%d{x by %s on %s%s\n\r",
        record->id, record->author, time_buf,
        record->group_id > 0 ? formatf(" {D[group %d]{x", record->group_id) : "");

    if (!IS_NULLSTR(record->comment))
        printf_to_char(ch, "{DComment:{x %s\n\r", record->comment);

    printf_to_char(ch, "\n\r{D%-25s %-8s %-20s %-20s{x\n\r",
        "Field", "Type", "Old", "New");
    printf_to_char(ch, "{D%.73s{x\n\r",
        "-------------------------------------------------------------------------");

    ITERATOR it;
    iterator_start(&it, record->changes);
    olc_committed_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        char old_buf[64], new_buf[64];
        format_json_brief(old_buf, sizeof(old_buf), change->old_value);
        format_json_brief(new_buf, sizeof(new_buf), change->new_value);

        printf_to_char(ch, " {Y%-24s{x %-8s %-20s {W%-20s{x\n\r",
            change->field_path,
            field_type_label(change->field_type),
            old_buf, new_buf);
    }
    iterator_stop(&it);

    printf_to_char(ch, "\n\r{x%d change%s. Use '{Whistory revert %d{x' to undo.\n\r",
        list_size(record->changes),
        list_size(record->changes) == 1 ? "" : "s",
        record->id);
}

static void do_history_revert(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit, olc_commit_history_t *history, char *argument)
{
    char arg_id[MIL], arg_confirm[MIL];
    argument = one_argument(argument, arg_id);
    argument = one_argument(argument, arg_confirm);

    if (!is_number(arg_id)) {
        send_to_char("Syntax: history revert <id> [confirm]\n\r", ch);
        return;
    }

    int id = atoi(arg_id);
    olc_commit_record_t *record = olc_commit_history_find(history, id);
    if (!record) {
        printf_to_char(ch, "No commit record with ID %d.\n\r", id);
        return;
    }

    /* Preview mode: show what would change */
    if (str_cmp(arg_confirm, "confirm")) {
        printf_to_char(ch, "{WRevert preview for commit #%d{x by %s:\n\r\n\r",
            record->id, record->author);

        printf_to_char(ch, "{D%-25s %-20s %-20s{x\n\r",
            "Field", "Current (new)", "Revert to (old)");
        printf_to_char(ch, "{D%.65s{x\n\r",
            "-----------------------------------------------------------------");

        int revertable = 0;
        int skipped = 0;
        ITERATOR it;
        iterator_start(&it, record->changes);
        olc_committed_change_t *change;
        while ((change = iterator_nextdata(&it)) != NULL) {
            bool can_revert = (change->field_type <= OLC_FIELD_MULTIFLAGS
                || change->field_type == OLC_FIELD_MULTILINE);
            char cur_buf[64], old_buf[64];
            format_json_brief(cur_buf, sizeof(cur_buf), change->new_value);
            format_json_brief(old_buf, sizeof(old_buf), change->old_value);

            if (can_revert) {
                printf_to_char(ch, " {Y%-24s{x %-20s -> {W%-20s{x\n\r",
                    change->field_path, cur_buf, old_buf);
                revertable++;
            } else {
                printf_to_char(ch, " {D%-24s %-20s   (skip: %s){x\n\r",
                    change->field_path, cur_buf,
                    field_type_label(change->field_type));
                skipped++;
            }
        }
        iterator_stop(&it);

        printf_to_char(ch, "\n\r%d field%s will be reverted",
            revertable, revertable == 1 ? "" : "s");
        if (skipped > 0)
            printf_to_char(ch, ", %d skipped (non-scalar)", skipped);
        printf_to_char(ch, ".\n\rType '{Whistory revert %d confirm{x' to proceed.\n\r", id);
        return;
    }

    /* Confirmed: stage reverse changes, then commit */
    olc_changeset_t *cs = olc_get_active_changeset(ch, def);
    if (!cs) {
        send_to_char("No active changeset — cannot revert.\n\r", ch);
        return;
    }

    if (olc_changeset_count(cs) > 0) {
        send_to_char("{RYou have pending changes.{x Commit or revert them first.\n\r", ch);
        return;
    }

    int reverted = 0;
    int skipped = 0;
    ITERATOR it;
    iterator_start(&it, record->changes);
    olc_committed_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        bool can_revert = (change->field_type <= OLC_FIELD_MULTIFLAGS
            || change->field_type == OLC_FIELD_MULTILINE);
        if (!can_revert) {
            skipped++;
            continue;
        }

        /* Stage the reverse: new_value becomes old, old_value becomes new */
        olc_changeset_add_change(cs, change->field_path, change->field_type,
            change->new_value, change->old_value);
        reverted++;
    }
    iterator_stop(&it);

    if (reverted == 0) {
        send_to_char("No revertable fields in this commit.\n\r", ch);
        return;
    }

    /* Delegate to olc_staged_cmd_commit() which handles:
     * archive -> commit -> area flag -> GMCP notification */
    char revert_comment[MIL];
    snprintf(revert_comment, sizeof(revert_comment), "Revert of commit #%d", id);
    olc_staged_cmd_commit(ch, def, pEdit, revert_comment);

    if (skipped > 0)
        printf_to_char(ch, "{D(%d non-scalar field%s skipped){x\n\r",
            skipped, skipped == 1 ? "" : "s");
}

void olc_staged_cmd_history(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit, char *argument)
{
    WNUM_LOAD wnum = olc_get_entity_wnum(def, pEdit);
    olc_commit_history_t *history = olc_commit_history_get_or_load(
        def->editor_type, wnum);

    if (!history || olc_commit_history_count(history) == 0) {
        if (IS_NULLSTR(argument) || is_number(argument)) {
            send_to_char("No commit history for this entity.\n\r", ch);
            return;
        }
    }

    char arg1[MIL];
    char *rest = one_argument(argument, arg1);

    /* "history revert <id> [confirm]" */
    if (!str_cmp(arg1, "revert")) {
        do_history_revert(ch, def, pEdit, history, rest);
        return;
    }

    /* "history <id>" — detail view */
    if (is_number(arg1)) {
        show_history_detail(ch, history, atoi(arg1));
        return;
    }

    /* "history" or "history <count>" — list view */
    show_history_list(ch, history, argument);
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

/* =========================================================================
 * Embedded Struct Snapshot Helpers
 * ========================================================================= */

/**
 * Get the staged JSON snapshot for an embedded struct.
 * Returns the pending new_value JSON, or NULL if no change is staged.
 * The returned JSON is borrowed — do NOT decref.
 */
json_t *olc_staged_embedded(olc_changeset_t *cs, const char *struct_name)
{
    return olc_staged_json(cs, struct_name);
}

/**
 * Modify a key within a staged embedded snapshot.
 * If no snapshot is staged yet, this is a no-op and returns false.
 * If the snapshot exists, sets key=value in the new_value JSON object.
 */
bool olc_staged_embedded_set(olc_changeset_t *cs, const char *struct_name,
                              const char *key, json_t *value)
{
    if (!cs) return false;

    olc_pending_change_t *change = olc_changeset_find_change(cs, struct_name);
    if (!change || !change->new_value) return false;

    json_object_set(change->new_value, key, value);
    cs->is_dirty = true;
    return true;
}

/**
 * Get the staged flag value for a field, or the live value if not staged.
 * Used by type commands to read the current effective flags before toggling.
 */
long olc_staged_flags_or(olc_changeset_t *cs, const char *field, long live_value)
{
    json_t *staged = olc_staged_json(cs, field);
    if (staged && json_is_integer(staged))
        return (long)json_integer_value(staged);
    return live_value;
}

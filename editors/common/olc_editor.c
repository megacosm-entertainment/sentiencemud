/**
 * @file olc_editor.c
 * @brief Unified OLC Editor Framework - Core Implementation
 *
 * Implements the editor lifecycle, permissions, change tracking,
 * audit logging, and theme system.
 *
 * @see olc_editor.h for API documentation
 */

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../mxp_links.h"
#include "../common.h"
#include "olc_editor.h"

/* Internal helper: check if MXP is available for a character */
static bool display_use_mxp(CHAR_DATA *ch)
{
    if (!ch || !ch->desc) return false;
    return isMXP(ch->desc) && IS_SET(ch->comm, COMM_MXP);
}

/* =========================================================================
 * Theme Definitions
 * ========================================================================= */

const OLC_EDITOR_THEME olc_theme_default = {
    .label          = "{Y",
    .value          = "{C",
    .unset          = "{D",
    .section        = "{G",
    .section_text   = "{W",
    .border         = "{G",
    .clickable      = "{W",
    .tab_active     = "{Y",
    .tab_inactive   = "{g",
    .header         = "{W",
    .flag_colors    = "YCCWGDD"
};

const OLC_EDITOR_THEME olc_theme_world = {
    .label          = "{Y",
    .value          = "{G",
    .unset          = "{D",
    .section        = "{g",
    .section_text   = "{W",
    .border         = "{g",
    .clickable      = "{W",
    .tab_active     = "{Y",
    .tab_inactive   = "{g",
    .header         = "{G",
    .flag_colors    = "YggWGDD"
};

const OLC_EDITOR_THEME olc_theme_entity = {
    .label          = "{Y",
    .value          = "{C",
    .unset          = "{D",
    .section        = "{B",
    .section_text   = "{W",
    .border         = "{B",
    .clickable      = "{W",
    .tab_active     = "{Y",
    .tab_inactive   = "{c",
    .header         = "{C",
    .flag_colors    = "YBBWCDD"
};

const OLC_EDITOR_THEME olc_theme_data = {
    .label          = "{c",
    .value          = "{W",
    .unset          = "{D",
    .section        = "{C",
    .section_text   = "{W",
    .border         = "{C",
    .clickable      = "{Y",
    .tab_active     = "{W",
    .tab_inactive   = "{c",
    .header         = "{W",
    .flag_colors    = "cCCWWDD"
};

const OLC_EDITOR_THEME olc_theme_scripting = {
    .label          = "{M",
    .value          = "{W",
    .unset          = "{D",
    .section        = "{m",
    .section_text   = "{W",
    .border         = "{m",
    .clickable      = "{Y",
    .tab_active     = "{W",
    .tab_inactive   = "{m",
    .header         = "{M",
    .flag_colors    = "MmmWWDD"
};

const OLC_EDITOR_THEME olc_theme_system = {
    .label          = "{R",
    .value          = "{W",
    .unset          = "{D",
    .section        = "{r",
    .section_text   = "{W",
    .border         = "{r",
    .clickable      = "{Y",
    .tab_active     = "{W",
    .tab_inactive   = "{r",
    .header         = "{R",
    .flag_colors    = "RrrWWDD"
};

const OLC_EDITOR_THEME olc_theme_building = {
    .label          = "{Y",
    .value          = "{W",
    .unset          = "{D",
    .section        = "{y",
    .section_text   = "{W",
    .border         = "{y",
    .clickable      = "{C",
    .tab_active     = "{W",
    .tab_inactive   = "{y",
    .header         = "{Y",
    .flag_colors    = "YyyWWDD"
};

/* =========================================================================
 * Editor Registry
 * ========================================================================= */

#define MAX_REGISTERED_EDITORS 64

static const OLC_EDITOR_DEF *registered_editors[MAX_REGISTERED_EDITORS];
static int registered_editor_count = 0;

void olc_register_editor(const OLC_EDITOR_DEF *def)
{
    if (!def) return;
    if (registered_editor_count >= MAX_REGISTERED_EDITORS) {
        log_stringf("olc_register_editor: registry full, cannot register '%s'",
                   def->name ? def->name : "unknown");
        return;
    }
    registered_editors[registered_editor_count++] = def;
}

const OLC_EDITOR_DEF *olc_find_editor_by_type(int editor_type)
{
    for (int i = 0; i < registered_editor_count; i++) {
        if (registered_editors[i]->editor_type == editor_type)
            return registered_editors[i];
    }
    return NULL;
}

const OLC_EDITOR_DEF *olc_find_editor_by_name(const char *name)
{
    if (IS_NULLSTR(name)) return NULL;

    for (int i = 0; i < registered_editor_count; i++) {
        if (!str_prefix(name, registered_editors[i]->name))
            return registered_editors[i];
    }
    return NULL;
}

/* =========================================================================
 * Theme Helpers
 * ========================================================================= */

const OLC_EDITOR_THEME *olc_get_theme(const OLC_EDITOR_DEF *def)
{
    if (def && def->theme)
        return def->theme;
    return &olc_theme_default;
}

/* =========================================================================
 * Permission Checking
 * ========================================================================= */

bool olc_editor_check_perm(CHAR_DATA *ch, const OLC_EDITOR_DEF *def, void *pEdit)
{
    if (!ch || !def) return false;

    /* NPCs never get editor access */
    if (IS_NPC(ch)) return false;

    /* Player-accessible editors have different rules */
    if (def->perm.flags & OLC_PERM_PLAYER) {
        /* Player editors rely solely on the custom callback */
        if (def->perm.flags & OLC_PERM_CUSTOM) {
            return def->perm.check_fn
                ? def->perm.check_fn(ch, pEdit)
                : false;
        }
        return true; /* PLAYER flag without custom check = open access */
    }

    /* All non-player editors require immortal status */
    if (!IS_IMMORTAL(ch)) return false;

    /* Check staff rank if required */
    if (def->perm.flags & OLC_PERM_STAFF_RANK) {
        if (get_staff_rank(ch) < def->perm.min_staff_rank)
            return false;
    }

    /* Check area security if required */
    if (def->perm.flags & OLC_PERM_AREA_SECURITY) {
        /* Need to get the area from the entity */
        AREA_DATA *area = NULL;
        if (def->get_area_fn && pEdit) {
            area = def->get_area_fn(pEdit);
        }
        if (area && !IS_BUILDER(ch, area))
            return false;
    }

    /* Check security level if required */
    if (def->perm.flags & OLC_PERM_SECURITY_LEVEL) {
        if (!ch->pcdata || ch->pcdata->security < def->perm.min_security)
            return false;
    }

    /* Custom permission check */
    if (def->perm.flags & OLC_PERM_CUSTOM) {
        if (def->perm.check_fn && !def->perm.check_fn(ch, pEdit))
            return false;
    }

    return true;
}

/* =========================================================================
 * Change Tracking
 * ========================================================================= */

void olc_mark_changed(CHAR_DATA *ch, const OLC_EDITOR_DEF *def)
{
    if (!ch || !ch->desc || !def) return;

    void *pEdit = ch->desc->pEdit;

    switch (def->change_mode) {
        case OLC_CHANGE_AREA_FLAG:
            if (def->get_area_fn && pEdit) {
                AREA_DATA *area = def->get_area_fn(pEdit);
                if (area) {
                    SET_BIT(area->area_flags, AREA_CHANGED);
                }
            }
            break;

        case OLC_CHANGE_CUSTOM:
            if (def->mark_changed_fn) {
                def->mark_changed_fn(ch, pEdit);
            }
            break;

        case OLC_CHANGE_EXPLICIT_SAVE:
            /* No automatic marking - editors with explicit save handle it
             * in their save command. Nothing to do here. */
            break;

        case OLC_CHANGE_NONE:
            break;
    }
}

/* =========================================================================
 * Audit Logging
 * ========================================================================= */

void olc_audit_log(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
                   const char *field, const char *old_val,
                   const char *new_val, long entity_id)
{
    if (!ch || !def) return;

    const char *char_name = ch->name ? ch->name : "Unknown";
    const char *editor_name = def->name ? def->name : "OLC";

    /* Log to the system log */
    log_stringf("OLC AUDIT: [%s] %s changed %s on entity %ld: '%s' -> '%s'",
               editor_name, char_name, field ? field : "?",
               entity_id,
               old_val ? old_val : "(null)",
               new_val ? new_val : "(null)");
}

void olc_audit_logf(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
                    long entity_id, const char *fmt, ...)
{
    char buf[OLC_AUDIT_MSG_LEN];
    va_list args;

    if (!ch || !def || !fmt) return;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    const char *char_name = ch->name ? ch->name : "Unknown";
    const char *editor_name = def->name ? def->name : "OLC";

    log_stringf("OLC AUDIT: [%s] %s on entity %ld: %s",
               editor_name, char_name, entity_id, buf);
}

/* =========================================================================
 * Editor Lifecycle
 * ========================================================================= */

void olc_editor_enter(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
                      void *pEdit, bool show_initial)
{
    if (!ch || !ch->desc || !def) return;

    /* Check permissions */
    if (!olc_editor_check_perm(ch, def, pEdit)) {
        send_to_char("You don't have permission to use this editor.\n\r", ch);
        return;
    }

    /* Set up the editor state on the descriptor */
    ch->desc->pEdit = pEdit;
    ch->desc->editor = def->editor_type;
    ch->desc->nEditTab = 0;

    /* Set max tabs from the definition */
    ch->desc->nMaxEditTabs = def->tabs.count;

    /* Show initial display if requested */
    if (show_initial && def->show_fn) {
        (*def->show_fn)(ch, "");
    }
}

/**
 * Internal: Handle tab switching for tabbed editors.
 * @return true if argument was consumed as a tab switch
 */
static bool editor_try_tab_switch(CHAR_DATA *ch, const char *argument,
                                  const OLC_EDITOR_DEF *def)
{
    char arg[MAX_INPUT_LENGTH];
    int tab_num;

    if (!def || def->tabs.count < 1) return false;
    if (IS_NULLSTR(argument)) return false;

    one_argument((char *)argument, arg);

    /* Numeric tab selection: "1", "2", "3", etc. */
    if (is_number(arg)) {
        tab_num = atoi(arg);
        if (tab_num >= 1 && tab_num <= def->tabs.count) {
            ch->desc->nEditTab = tab_num - 1;
            return true;
        }
        return false;
    }

    /* "tab <name>" command */
    if (!str_cmp(arg, "tab")) {
        char tab_name[MAX_INPUT_LENGTH];
        char *rest = (char *)argument;
        rest = one_argument(rest, arg); /* skip "tab" */
        one_argument(rest, tab_name);

        if (tab_name[0] == '\0') {
            send_to_char("Switch to which tab?\n\r", ch);
            return true;
        }

        for (int i = 0; i < def->tabs.count; i++) {
            const OLC_EDITOR_TAB *tab = &def->tabs.tabs[i];
            if ((tab->name && !str_prefix(tab_name, tab->name)) ||
                (tab->short_name && !str_prefix(tab_name, tab->short_name))) {
                ch->desc->nEditTab = i;
                return true;
            }
        }

        send_to_char("No such tab.\n\r", ch);
        return true;
    }

    return false;
}

/* =========================================================================
 * Change History (Per-Entity Audit Trail)
 * ========================================================================= */

/**
 * Destructor for OLC_CHANGE_ENTRY — called by LLIST on list_destroy().
 */
static void change_entry_destroy(void *data)
{
    OLC_CHANGE_ENTRY *entry = (OLC_CHANGE_ENTRY *)data;
    if (!entry) return;

    free_string(entry->author);
    free_string(entry->field);
    free_string(entry->old_value);
    free_string(entry->new_value);
    free(entry);
}

OLC_CHANGE_HISTORY *olc_history_new(void)
{
    OLC_CHANGE_HISTORY *history = calloc(1, sizeof(OLC_CHANGE_HISTORY));
    if (!history) return NULL;

    history->entries = list_createx(false, NULL, change_entry_destroy);
    history->next_id = 1;
    history->max_entries = OLC_MAX_HISTORY_ENTRIES;
    return history;
}

void olc_history_free(OLC_CHANGE_HISTORY *history)
{
    if (!history) return;

    if (history->entries) {
        list_destroy(history->entries);
    }
    free(history);
}

void olc_history_record(OLC_CHANGE_HISTORY *history, CHAR_DATA *ch,
                        const char *field, const char *old_val,
                        const char *new_val)
{
    OLC_CHANGE_ENTRY *entry;

    if (!history || !ch || IS_NULLSTR(field)) return;

    /* Create the new entry */
    entry = calloc(1, sizeof(OLC_CHANGE_ENTRY));
    if (!entry) return;

    entry->author    = str_dup(ch->name ? ch->name : "Unknown");
    entry->timestamp = current_time;
    entry->id        = history->next_id++;
    entry->field     = str_dup(field);
    entry->old_value = str_dup(old_val ? old_val : "(none)");
    entry->new_value = str_dup(new_val ? new_val : "(none)");

    /* Add to front of list (newest first) */
    list_addlink(history->entries, entry);

    /* Evict oldest if over capacity */
    while (list_size(history->entries) > history->max_entries) {
        /* Remove from the tail (oldest) */
        ITERATOR it;
        OLC_CHANGE_ENTRY *oldest = NULL;

        iterator_start(&it, history->entries);
        while (iterator_nextdata(&it)) {
            /* Just iterate to the end */
        }
        /* Get the last element by iterating through */
        iterator_stop(&it);

        /* Walk to find the last element */
        int size = list_size(history->entries);
        if (size > 0) {
            int count = 0;
            iterator_start(&it, history->entries);
            OLC_CHANGE_ENTRY *cur;
            while ((cur = (OLC_CHANGE_ENTRY *)iterator_nextdata(&it))) {
                count++;
                if (count == size) {
                    oldest = cur;
                }
            }
            iterator_stop(&it);

            if (oldest) {
                list_remlink(history->entries, oldest, true);
            }
        }
    }
}

void olc_history_show(CHAR_DATA *ch, const OLC_CHANGE_HISTORY *history,
                      const char *argument, const OLC_EDITOR_THEME *theme)
{
    BUFFER *buffer;
    ITERATOR it;
    OLC_CHANGE_ENTRY *entry;
    int count = 0;
    int limit = 0;
    const char *field_filter = NULL;
    const char *border_col;
    const char *label_col;
    const char *value_col;

    if (!theme) theme = &olc_theme_default;
    border_col = theme->border;
    label_col  = theme->label;
    value_col  = theme->value;

    if (!history || !history->entries || list_size(history->entries) == 0) {
        send_to_char("No change history available for this entity.\n\r", ch);
        return;
    }

    /* Parse argument: number = limit, text = field name filter */
    if (!IS_NULLSTR(argument)) {
        if (is_number((char *)argument)) {
            limit = atoi(argument);
        } else {
            field_filter = argument;
        }
    }

    buffer = new_buf();

    /* Table header */
    bprintf(buffer,
        "%s+------+--------------------+--------------------+-------------------------------+{x\n\r",
        border_col);
    bprintf(buffer,
        "%s| %sID{x   | %sAuthor{x             | %sField{x              | %sDate & Time{x                  %s|{x\n\r",
        border_col, label_col, label_col, label_col, label_col, border_col);
    bprintf(buffer,
        "%s+------+--------------------+--------------------+-------------------------------+{x\n\r",
        border_col);

    /* Show entries (list is newest-first) */
    iterator_start(&it, history->entries);
    while ((entry = (OLC_CHANGE_ENTRY *)iterator_nextdata(&it))) {
        char time_buf[64];
        struct tm *time_info;

        /* Apply field filter if set */
        if (field_filter && str_prefix(field_filter, entry->field)) {
            continue;
        }

        /* Apply limit if set */
        if (limit > 0 && count >= limit) {
            break;
        }

        /* Format timestamp */
        time_info = localtime(&entry->timestamp);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", time_info);

        /* Render the row — make ID clickable via MXP to `view <id>` */
        if (display_use_mxp(ch)) {
            char id_str[16];
            char view_cmd[32];
            snprintf(id_str, sizeof(id_str), "%-5d", entry->id);
            snprintf(view_cmd, sizeof(view_cmd), "view %d", entry->id);
            bprintf(buffer, "%s| {x", border_col);
            mxp_command_link(ch->desc, buffer, view_cmd, "View change details", id_str);
            bprintf(buffer, " %s|{x %-18.18s %s|{x %s%-18.18s{x %s|{x %-29s %s|{x\n\r",
                border_col,
                entry->author,
                border_col,
                value_col, entry->field,
                border_col,
                time_buf,
                border_col);
        } else {
            bprintf(buffer,
                "%s| %s%-5d{x %s|{x %-18.18s %s|{x %s%-18.18s{x %s|{x %-29s %s|{x\n\r",
                border_col,
                value_col, entry->id,
                border_col,
                entry->author,
                border_col,
                value_col, entry->field,
                border_col,
                time_buf,
                border_col);
        }

        count++;
    }
    iterator_stop(&it);

    /* Table footer */
    bprintf(buffer,
        "%s+------+--------------------+--------------------+-------------------------------+{x\n\r",
        border_col);

    if (count == 0) {
        bprintf(buffer, "\n\rNo matching history entries found.\n\r");
    } else {
        bprintf(buffer, "\n\rShowing %d of %d entries. Use '%sview <id>{x' to see details.\n\r",
            count, list_size(history->entries), value_col);
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}

void olc_history_view(CHAR_DATA *ch, const OLC_CHANGE_HISTORY *history,
                      int id, const OLC_EDITOR_THEME *theme)
{
    ITERATOR it;
    OLC_CHANGE_ENTRY *entry;
    OLC_CHANGE_ENTRY *found = NULL;
    BUFFER *buffer;
    char time_buf[64];
    struct tm *time_info;
    const char *border_col;
    const char *label_col;
    const char *value_col;

    if (!theme) theme = &olc_theme_default;
    border_col = theme->border;
    label_col  = theme->label;
    value_col  = theme->value;

    if (!history || !history->entries) {
        send_to_char("No change history available.\n\r", ch);
        return;
    }

    /* Find the entry with the given ID */
    iterator_start(&it, history->entries);
    while ((entry = (OLC_CHANGE_ENTRY *)iterator_nextdata(&it))) {
        if (entry->id == id) {
            found = entry;
            break;
        }
    }
    iterator_stop(&it);

    if (!found) {
        send_to_char("No change entry found with that ID.\n\r", ch);
        return;
    }

    /* Format timestamp */
    time_info = localtime(&found->timestamp);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", time_info);

    buffer = new_buf();

    /* Header */
    bprintf(buffer,
        "%s+----------------------------------------------------------------------------+{x\n\r",
        border_col);
    bprintf(buffer,
        "%s| %sChange #%-5d{x%64s%s|{x\n\r",
        border_col, label_col, found->id, "", border_col);
    bprintf(buffer,
        "%s+----------------------------------------------------------------------------+{x\n\r",
        border_col);

    /* Author and date */
    bprintf(buffer,
        "%s| %sAuthor:{x  %-67s %s|{x\n\r",
        border_col, label_col, found->author, border_col);
    bprintf(buffer,
        "%s| %sDate:{x    %-67s %s|{x\n\r",
        border_col, label_col, time_buf, border_col);

    /* Field and values */
    bprintf(buffer,
        "%s+------------------------+---------------------------------------------------+{x\n\r",
        border_col);
    bprintf(buffer,
        "%s| %sField{x                | %s%-49.49s{x %s|{x\n\r",
        border_col, label_col, value_col, found->field, border_col);
    bprintf(buffer,
        "%s+------------------------+---------------------------------------------------+{x\n\r",
        border_col);
    bprintf(buffer,
        "%s| %sOld Value{x            | %-49.49s %s|{x\n\r",
        border_col, label_col, found->old_value, border_col);
    bprintf(buffer,
        "%s| %sNew Value{x            | %-49.49s %s|{x\n\r",
        border_col, label_col, found->new_value, border_col);
    bprintf(buffer,
        "%s+------------------------+---------------------------------------------------+{x\n\r",
        border_col);

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}

void olc_editor_interp(CHAR_DATA *ch, char *argument, const OLC_EDITOR_DEF *def)
{
    char command[MAX_INPUT_LENGTH];
    char arg_for_interpret[MAX_STRING_LENGTH];
    char working_buf[MAX_STRING_LENGTH];
    char *rest;
    int cmd_index;

    if (!ch || !ch->desc || !def) return;

    /* --- Permission check --- */
    if (!olc_editor_check_perm(ch, def, ch->desc->pEdit)) {
        send_to_char(formatf("%s: Insufficient permissions.\n\r",
                    def->name ? def->name : "Editor"), ch);
        edit_done(ch);
        return;
    }

    /* --- Sanitize input --- */
    smash_tilde(argument);
    strcpy(arg_for_interpret, argument);
    strcpy(working_buf, argument);
    rest = working_buf;
    rest = one_argument(rest, command);

    /* --- "done" command --- */
    if (!str_cmp(command, "done")) {
        edit_done(ch);
        return;
    }

    /* --- Built-in "history" command (if editor supports change history) --- */
    if (!str_cmp(command, "history") && def->get_history_fn) {
        OLC_CHANGE_HISTORY *hist = def->get_history_fn(ch->desc->pEdit);
        olc_history_show(ch, hist, rest, olc_get_theme(def));
        return;
    }

    /* --- Built-in "view" command (if editor supports change history) --- */
    if (!str_cmp(command, "view") && def->get_history_fn) {
        if (IS_NULLSTR(rest) || !is_number(rest)) {
            send_to_char("Syntax: view <change_id>\n\r", ch);
            return;
        }
        OLC_CHANGE_HISTORY *hist = def->get_history_fn(ch->desc->pEdit);
        olc_history_view(ch, hist, atoi(rest), olc_get_theme(def));
        return;
    }

    /* --- Update last OLC command timestamp --- */
    if (ch->pcdata && ch->pcdata->immortal) {
        ch->pcdata->immortal->last_olc_command = current_time;
    }

    /* --- Tab switching (if editor has tabs) --- */
    if (def->tabs.count > 0 && editor_try_tab_switch(ch, argument, def)) {
        /* Tab was switched - redisplay */
        if (def->show_fn) {
            (*def->show_fn)(ch, "");
        }
        return;
    }

    /* --- Empty input → show editor --- */
    if (command[0] == '\0') {
        if (def->show_fn) {
            (*def->show_fn)(ch, rest);
        } else {
            send_to_char("No display function available for this editor.\n\r", ch);
        }
        return;
    }

    /* --- Command table lookup --- */
    if (def->cmd_table) {
        for (cmd_index = 0; def->cmd_table[cmd_index].name != NULL; cmd_index++) {
            if (!str_prefix(command, def->cmd_table[cmd_index].name)) {
                /* Per-command permission check */
                if (!olc_check_command_perm(ch,
                        def->cmd_table[cmd_index].min_staff_rank,
                        def->cmd_table[cmd_index].name)) {
                    return;
                }

                /* Execute the command */
                if ((*def->cmd_table[cmd_index].olc_fun)(ch, rest)) {
                    /* Command returned true = data was changed */
                    olc_mark_changed(ch, def);

                    /* Audit log if enabled */
                    if (def->audit_changes) {
                        olc_audit_logf(ch, def, 0,
                            "command '%s' with args '%s'",
                            def->cmd_table[cmd_index].name,
                            rest ? rest : "");
                    }
                }
                return;
            }
        }
    }

    /* --- Fallback to game interpreter --- */
    interpret(ch, arg_for_interpret);
}

/* =========================================================================
 * Per-Command / Per-Argument Permission Utilities
 * ========================================================================= */

bool olc_check_command_perm(CHAR_DATA *ch, int min_staff_rank,
                            const char *cmd_name)
{
    if (min_staff_rank <= 0)
        return true;  /* 0 = unrestricted (inherits editor-level perm) */

    if (get_staff_rank(ch) >= min_staff_rank)
        return true;

    send_to_char(formatf(
        "You do not have sufficient rank to use '%s'.\n\r",
        cmd_name ? cmd_name : "that command"), ch);
    return false;
}

bool olc_check_perm(CHAR_DATA *ch, const OLC_EDITOR_PERM *perm,
                    void *pEdit, const char *label, bool silent)
{
    if (!ch || !perm) return false;

    /* NPC check */
    if (IS_NPC(ch)) {
        if (!silent)
            send_to_char("NPCs cannot use OLC.\n\r", ch);
        return false;
    }

    /* Player-mode permission (skip immortal requirement) */
    if (perm->flags & OLC_PERM_PLAYER) {
        if (perm->flags & OLC_PERM_CUSTOM) {
            if (!perm->check_fn || !perm->check_fn(ch, pEdit)) {
                if (!silent)
                    send_to_char(formatf(
                        "You do not have permission to edit '%s'.\n\r",
                        label ? label : "that"), ch);
                return false;
            }
        }
        return true;
    }

    /* Standard immortal gate */
    if (!IS_IMMORTAL(ch)) {
        if (!silent)
            send_to_char(formatf(
                "You do not have permission to edit '%s'.\n\r",
                label ? label : "that"), ch);
        return false;
    }

    /* Staff rank check */
    if (perm->flags & OLC_PERM_STAFF_RANK) {
        if (get_staff_rank(ch) < perm->min_staff_rank) {
            if (!silent)
                send_to_char(formatf(
                    "Your staff rank is too low to edit '%s'.\n\r",
                    label ? label : "that"), ch);
            return false;
        }
    }

    /* Security level check */
    if (perm->flags & OLC_PERM_SECURITY_LEVEL) {
        if (!ch->pcdata || ch->pcdata->security < perm->min_security) {
            if (!silent)
                send_to_char(formatf(
                    "Your security level is too low to edit '%s'.\n\r",
                    label ? label : "that"), ch);
            return false;
        }
    }

    /* Custom callback */
    if (perm->flags & OLC_PERM_CUSTOM) {
        if (perm->check_fn && !perm->check_fn(ch, pEdit)) {
            if (!silent)
                send_to_char(formatf(
                    "You do not have permission to edit '%s'.\n\r",
                    label ? label : "that"), ch);
            return false;
        }
    }

    return true;
}

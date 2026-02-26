/***************************************************************************
 *  soedit.c — OLC Song Editor                                            *
 *                                                                         *
 *  In-game editor for SONG_DATA definitions. Allows immortals to view    *
 *  and modify song properties including name, level, mana cost, target,  *
 *  beats, and spell effects.                                             *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../recycle.h"
#include "../../song_data.h"
#include "../../skill_data.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"
#include "../../io/json/json_olc.h"

/***************************************************************************
 * History Helpers                                                         *
 ***************************************************************************/

static OLC_CHANGE_HISTORY *soedit_get_history(void *pEdit)
{
    SONG_DATA *song = (SONG_DATA *)pEdit;
    return song ? (OLC_CHANGE_HISTORY *)song->olc_history : NULL;
}

static OLC_CHANGE_HISTORY *soedit_ensure_history(SONG_DATA *song)
{
    if (!song) return NULL;
    if (!song->olc_history)
        song->olc_history = olc_history_load(OLC_HIST_SONG, song->name);
    if (!song->olc_history)
        song->olc_history = olc_history_new();
    return (OLC_CHANGE_HISTORY *)song->olc_history;
}

/**
 * Convenience: record a change and mark the song dirty for persistence.
 */
static void soedit_record(SONG_DATA *song, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    olc_history_record(soedit_ensure_history(song), ch,
        field, old_val, new_val);
    olc_history_mark_dirty(OLC_HIST_SONG, song->name,
        (OLC_CHANGE_HISTORY *)song->olc_history);
}

/**
 * Generic callback wrapper for olc_cmd_* helpers.
 */
static void soedit_record_cb(void *ctx, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    soedit_record((SONG_DATA *)ctx, ch, field, old_val, new_val);
}

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type soedit_table[] =
{
    { "?",              show_help           },
    { "beats",          soedit_beats        },
    { "commands",       show_commands       },
    { "level",          soedit_level        },
    { "list",           soedit_list         },
    { "mana",           soedit_mana         },
    { "name",           soedit_name         },
    { "save",           soedit_save         },
    { "show",           soedit_show         },
    { "spell",          soedit_spell        },
    { "target",         soedit_target       },
    { NULL,             0                   }
};

/***************************************************************************
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF soedit_def = {
    .name           = "SoEdit",
    .editor_type    = ED_SONG,
    .cmd_table      = soedit_table,
    .show_fn        = soedit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = true,
    .get_history_fn = soedit_get_history,
};

/***************************************************************************
 * Entry Point                                                             *
 ***************************************************************************/

/**
 * do_soedit - Enter the song editor
 *
 * Syntax:
 *   soedit list           - List all songs
 *   soedit <name>         - Edit a song by name
 */
void do_soedit(CHAR_DATA *ch, char *argument)
{
    SONG_DATA *song;
    char arg1[MAX_INPUT_LENGTH];

    if (IS_NPC(ch))
        return;

    if (!olc_editor_check_perm(ch, &soedit_def, NULL)) {
        send_to_char("You don't have permission to edit songs.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: soedit <song name>\n\r", ch);
        send_to_char("        soedit list\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "list")) {
        soedit_list(ch, argument);
        return;
    }

    song = song_lookup(arg1);

    if (!song) {
        send_to_char("No song found with that name.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &soedit_def, song, true);
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * soedit - Command interpreter for the song editor
 */
void soedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &soedit_def);
}

/***************************************************************************
 * Show                                                                    *
 ***************************************************************************/

SOEDIT(soedit_show)
{
    SONG_DATA *song;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&soedit_def);
    OLC_LAYOUT_CTX *ctx;

    EDIT_SONG(ch, song);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "SoEdit", song->name,
        formatf("UID %d", song->uid), &soedit_def);

    olc_display_string(ctx, theme, "Name:", "name", song->name);
    olc_display_number(ctx, theme, "Level:", "level", song->level);
    olc_display_pair(ctx, theme,
        "Mana:", "mana", formatf("%d", song->mana),
        "Beats:", "beats", formatf("%d", song->beats));
    olc_display_type(ctx, theme, "Target:", "target",
        song_target_types, song->target);

    olc_display_section(ctx, theme, "Spells");

    olc_display_string(ctx, theme, "Spell 1:", "spell1", song->spell1);
    olc_display_string(ctx, theme, "Spell 2:", "spell2", song->spell2);
    olc_display_string(ctx, theme, "Spell 3:", "spell3", song->spell3);

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

/***************************************************************************
 * Command Functions                                                       *
 ***************************************************************************/

SOEDIT(soedit_list)
{
    LLIST *songs = song_get_list();
    ITERATOR it;
    SONG_DATA *song;
    BUFFER *buf;
    int count = 0;

    buf = new_buf();
    add_buf(buf, formatf("{Y%-5s %-25s %-5s %-5s %-5s %-15s{x\n\r",
            "UID", "Name", "Level", "Mana", "Beats", "Target"));
    add_buf(buf, formatf("{Y%-5s %-25s %-5s %-5s %-5s %-15s{x\n\r",
            "-----", "-------------------------", "-----", "-----",
            "-----", "---------------"));

    if (songs) {
        iterator_start(&it, songs);
        while ((song = (SONG_DATA *)iterator_nextdata(&it))) {
            if (argument[0] && str_prefix(argument, song->name))
                continue;

            add_buf(buf, formatf("%-5d %-25s %-5d %-5d %-5d %-15s\n\r",
                    song->uid,
                    song->name,
                    song->level,
                    song->mana,
                    song->beats,
                    flag_string(song_target_types, song->target)));
            count++;
        }
        iterator_stop(&it);
    }

    add_buf(buf, formatf("\n\r%d song%s listed.\n\r",
            count, count == 1 ? "" : "s"));
    page_to_char(buf_string(buf), ch);
    free_buf(buf);
    return false;
}

SOEDIT(soedit_name)
{
    SONG_DATA *song;
    EDIT_SONG(ch, song);
    return olc_cmd_string(ch, argument, "Name", NULL, &song->name,
        OLC_STR_DEFAULT, song, soedit_record_cb);
}

SOEDIT(soedit_level)
{
    SONG_DATA *song;
    EDIT_SONG(ch, song);
    return olc_cmd_number(ch, argument, "Level", NULL, &song->level,
        1, MAX_LEVEL, song, soedit_record_cb);
}

SOEDIT(soedit_mana)
{
    SONG_DATA *song;
    EDIT_SONG(ch, song);
    return olc_cmd_number_i16(ch, argument, "Mana", NULL, &song->mana,
        0, 9999, song, soedit_record_cb);
}

SOEDIT(soedit_beats)
{
    SONG_DATA *song;
    EDIT_SONG(ch, song);
    return olc_cmd_number_i16(ch, argument, "Beats", NULL, &song->beats,
        0, 999, song, soedit_record_cb);
}

SOEDIT(soedit_target)
{
    SONG_DATA *song;
    EDIT_SONG(ch, song);
    return olc_cmd_type_set_i16(ch, argument, "Target", NULL, &song->target,
        song_target_types, song, soedit_record_cb);
}

/**
 * soedit_spell - Set spell effects for a song
 *
 * Syntax:
 *   spell 1|2|3 <spell name>
 *   spell 1|2|3 none
 */
SOEDIT(soedit_spell)
{
    SONG_DATA *song;
    char arg[MAX_INPUT_LENGTH];
    char **slot;

    EDIT_SONG(ch, song);

    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0') {
        send_to_char("Syntax: spell 1|2|3 <spell name>\n\r", ch);
        send_to_char("        spell 1|2|3 none\n\r", ch);
        return false;
    }

    if (!str_cmp(arg, "1"))
        slot = &song->spell1;
    else if (!str_cmp(arg, "2"))
        slot = &song->spell2;
    else if (!str_cmp(arg, "3"))
        slot = &song->spell3;
    else {
        send_to_char("Spell slot must be 1, 2, or 3.\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "none") || !str_cmp(argument, "clear")) {
        olc_history_record(soedit_ensure_history(song), ch,
            formatf("spell%s", arg), *slot, "(none)");
        olc_history_mark_dirty(OLC_HIST_SONG, song->name,
            (OLC_CHANGE_HISTORY *)song->olc_history);
        free_string(*slot);
        *slot = &str_empty[0];
        send_to_char(formatf("Spell %s cleared.\n\r", arg), ch);
        return true;
    }

    /* Validate the spell exists */
    SKILL_DATA *sk = skill_find(argument);
    if (!sk)
        sk = skill_search(argument);

    if (!sk || !sk->isspell) {
        send_to_char("No spell found with that name.\n\r", ch);
        return false;
    }

    olc_history_record(soedit_ensure_history(song), ch,
        formatf("spell%s", arg), *slot, sk->name);
    olc_history_mark_dirty(OLC_HIST_SONG, song->name,
        (OLC_CHANGE_HISTORY *)song->olc_history);
    free_string(*slot);
    *slot = str_dup(sk->name);
    send_to_char(formatf("Spell %s set to '%s'.\n\r", arg, sk->name), ch);
    return true;
}

SOEDIT(soedit_save)
{
    SONG_DATA *song;
    EDIT_SONG(ch, song);

    save_songs();
    olc_history_flush(OLC_HIST_SONG, song->name,
        (OLC_CHANGE_HISTORY *)song->olc_history);
    send_to_char("Songs saved to JSON file.\n\r", ch);
    return false;
}

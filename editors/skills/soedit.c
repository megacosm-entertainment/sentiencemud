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

    ch->pcdata->immortal->last_olc_command = current_time;
    olc_set_editor(ch, ED_SONG, song);
    soedit_show(ch, "");
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * soedit - Command interpreter for the song editor
 */
void soedit(CHAR_DATA *ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_STRING_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (!str_cmp(command, "done")) {
        edit_done(ch);
        return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;

    if (command[0] == '\0') {
        soedit_show(ch, argument);
        return;
    }

    for (cmd = 0; soedit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, soedit_table[cmd].name)) {
            (*soedit_table[cmd].olc_fun)(ch, argument);
            return;
        }
    }

    interpret(ch, arg);
}

/***************************************************************************
 * Show                                                                    *
 ***************************************************************************/

SOEDIT(soedit_show)
{
    SONG_DATA *song;
    BUFFER *buf;

    EDIT_SONG(ch, song);

    buf = new_buf();

    add_buf(buf, formatf("{Y=== Song Editor ==={x\n\r"));
    add_buf(buf, formatf("{cUID:{x   %d\n\r", song->uid));
    add_buf(buf, formatf("{cName:{x  %s\n\r", song->name));
    add_buf(buf, formatf("{cLevel:{x %d\n\r", song->level));
    add_buf(buf, formatf("{cMana:{x  %-5d  {cBeats:{x %-5d\n\r",
            song->mana, song->beats));
    add_buf(buf, formatf("{cTarget:{x %s\n\r",
            flag_string(song_target_types, song->target)));

    add_buf(buf, formatf("{cSpell 1:{x %s\n\r",
            song->spell1 ? song->spell1 : "(none)"));
    add_buf(buf, formatf("{cSpell 2:{x %s\n\r",
            song->spell2 ? song->spell2 : "(none)"));
    add_buf(buf, formatf("{cSpell 3:{x %s\n\r",
            song->spell3 ? song->spell3 : "(none)"));

    page_to_char(buf_string(buf), ch);
    free_buf(buf);
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

    if (argument[0] == '\0') {
        send_to_char("Syntax: name <new name>\n\r", ch);
        return false;
    }

    free_string(song->name);
    song->name = str_dup(argument);
    send_to_char("Song name set.\n\r", ch);
    return true;
}

SOEDIT(soedit_level)
{
    SONG_DATA *song;
    int value;

    EDIT_SONG(ch, song);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: level <number>\n\r", ch);
        return false;
    }

    value = atoi(argument);
    if (value < 1 || value > MAX_LEVEL) {
        send_to_char(formatf("Level must be between 1 and %d.\n\r", MAX_LEVEL), ch);
        return false;
    }

    song->level = value;
    send_to_char("Level set.\n\r", ch);
    return true;
}

SOEDIT(soedit_mana)
{
    SONG_DATA *song;
    int value;

    EDIT_SONG(ch, song);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: mana <number>\n\r", ch);
        return false;
    }

    value = atoi(argument);
    if (value < 0 || value > 9999) {
        send_to_char("Mana must be between 0 and 9999.\n\r", ch);
        return false;
    }

    song->mana = value;
    send_to_char("Mana cost set.\n\r", ch);
    return true;
}

SOEDIT(soedit_beats)
{
    SONG_DATA *song;
    int value;

    EDIT_SONG(ch, song);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: beats <number>\n\r", ch);
        return false;
    }

    value = atoi(argument);
    if (value < 0 || value > 999) {
        send_to_char("Beats must be between 0 and 999.\n\r", ch);
        return false;
    }

    song->beats = value;
    send_to_char("Beats set.\n\r", ch);
    return true;
}

SOEDIT(soedit_target)
{
    SONG_DATA *song;
    int value;

    EDIT_SONG(ch, song);

    if (argument[0] == '\0') {
        send_to_char("Syntax: target <target type>\n\r", ch);
        show_help(ch, "target");
        return false;
    }

    value = flag_value(song_target_types, argument);
    if (value == NO_FLAG) {
        send_to_char("Invalid target type.\n\r", ch);
        return false;
    }

    song->target = value;
    send_to_char("Target set.\n\r", ch);
    return true;
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

    free_string(*slot);
    *slot = str_dup(sk->name);
    send_to_char(formatf("Spell %s set to '%s'.\n\r", arg, sk->name), ch);
    return true;
}

SOEDIT(soedit_save)
{
    save_songs();
    send_to_char("Songs saved to JSON file.\n\r", ch);
    return false;
}

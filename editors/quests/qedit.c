#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../merc.h"

static int qedit_parse_expiry_modes(const char *input)
{
    char buf[MSL];
    char *cursor;
    int modes = QUEST_EXPIRY_NONE;

    if (IS_NULLSTR(input) || !str_cmp(input, "none"))
        return QUEST_EXPIRY_NONE;

    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    for (cursor = buf; *cursor; ++cursor) {
        if (*cursor == '+' || *cursor == ',' || *cursor == '|')
            *cursor = ' ';
    }

    cursor = strtok(buf, " \t\r\n");
    while (cursor) {
        if (!str_cmp(cursor, "manual") || !str_cmp(cursor, "manual_area") || !str_cmp(cursor, "levequest"))
            modes |= QUEST_EXPIRY_MANUAL_AREA;
        else if (!str_cmp(cursor, "wall") || !str_cmp(cursor, "calendar") || !str_cmp(cursor, "wall_time"))
            modes |= QUEST_EXPIRY_WALL_TIME;
        else if (!str_cmp(cursor, "countdown") || !str_cmp(cursor, "timer"))
            modes |= QUEST_EXPIRY_COUNTDOWN;

        cursor = strtok(NULL, " \t\r\n");
    }

    return modes;
}

static void qedit_format_expiry_modes(int modes, char *buf, size_t buflen)
{
    bool first = true;

    if (buflen == 0)
        return;

    buf[0] = '\0';

    if (modes == QUEST_EXPIRY_NONE) {
        strncpy(buf, "none", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    if (modes & QUEST_EXPIRY_MANUAL_AREA) {
        strncat(buf, first ? "manual" : "+manual", buflen - strlen(buf) - 1);
        first = false;
    }
    if (modes & QUEST_EXPIRY_WALL_TIME) {
        strncat(buf, first ? "wall" : "+wall", buflen - strlen(buf) - 1);
        first = false;
    }
    if (modes & QUEST_EXPIRY_COUNTDOWN)
        strncat(buf, first ? "countdown" : "+countdown", buflen - strlen(buf) - 1);
}

void do_qedit(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *victim = NULL;
    AREA_DATA *area;
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MSL];
    char mode_buf[64];
    time_t now = current_time;

    if (!ch || !ch->pcdata || IS_NPC(ch))
        return;

    if (!IS_IMPLEMENTOR(ch)) {
        send_to_char("QEdit: Insufficient security.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    strncpy(arg3, argument, sizeof(arg3) - 1);
    arg3[sizeof(arg3) - 1] = '\0';

    if (IS_NULLSTR(arg1) || IS_NULLSTR(arg2)) {
        send_to_char("Syntax:\n\r", ch);
        send_to_char("  qedit <player> show\n\r", ch);
        send_to_char("  qedit <player> mode <none|manual+wall+countdown>\n\r", ch);
        send_to_char("  qedit <player> area <here|none|<uid>|<name>>\n\r", ch);
        send_to_char("  qedit <player> wall <none|+minutes|unix_timestamp>\n\r", ch);
        send_to_char("  qedit <player> countdown <none|minutes>\n\r", ch);
        send_to_char("  qedit <player> clear\n\r", ch);
        return;
    }

    victim = get_char_world(ch, arg1);
    if (!victim || IS_NPC(victim) || !victim->pcdata) {
        send_to_char("QEdit: Player must be online and valid.\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "show")) {
        char wall_buf[MSL];
        char area_buf[MSL];

        qedit_format_expiry_modes(victim->quest_runtime.expiry_modes, mode_buf, sizeof(mode_buf));

        if (victim->quest_runtime.expires_at > 0)
            strftime(wall_buf, sizeof(wall_buf), "%Y-%m-%d %H:%M:%S", localtime(&victim->quest_runtime.expires_at));
        else
            strcpy(wall_buf, "none");

        if (victim->quest_runtime.manual_trigger_area_uid > 0) {
            area = get_area_index(victim->quest_runtime.manual_trigger_area_uid);
            if (area)
                sprintf(area_buf, "%ld (%s)", victim->quest_runtime.manual_trigger_area_uid, area->name);
            else
                sprintf(area_buf, "%ld", victim->quest_runtime.manual_trigger_area_uid);
        } else {
            strcpy(area_buf, "none");
        }

        printf_to_char(ch, "QEdit: %s\n\r", victim->name);
        printf_to_char(ch, "  modes: %s\n\r", mode_buf);
        printf_to_char(ch, "  area : %s\n\r", area_buf);
        printf_to_char(ch, "  wall : %s\n\r", wall_buf);
        printf_to_char(ch, "  countdown: %d minute%s\n\r",
            victim->quest_runtime.expiry_countdown_minutes,
            victim->quest_runtime.expiry_countdown_minutes == 1 ? "" : "s");
        printf_to_char(ch, "  manual_ready: %s\n\r",
            quest_runtime_manual_trigger_ready(victim) ? "yes" : "no");
        printf_to_char(ch, "  expired_now : %s\n\r",
            quest_runtime_is_expired(victim, now) ? "yes" : "no");
        return;
    }

    if (!str_prefix(arg2, "clear")) {
        victim->quest_runtime.expiry_modes = QUEST_EXPIRY_NONE;
        victim->quest_runtime.expires_at = 0;
        victim->quest_runtime.expiry_countdown_minutes = 0;
        victim->quest_runtime.manual_trigger_area_uid = 0;
        victim->countdown = 0;
        send_to_char("QEdit: Expiration policy cleared.\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "mode")) {
        int modes;

        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: mode requires a value.\n\r", ch);
            return;
        }

        modes = qedit_parse_expiry_modes(arg3);
        victim->quest_runtime.expiry_modes = modes;
        qedit_format_expiry_modes(modes, mode_buf, sizeof(mode_buf));
        printf_to_char(ch, "QEdit: modes set to %s.\n\r", mode_buf);
        return;
    }

    if (!str_prefix(arg2, "area")) {
        char area_arg[MIL];

        argument = one_argument(argument, area_arg);
        if (IS_NULLSTR(area_arg)) {
            send_to_char("QEdit: area requires a value.\n\r", ch);
            return;
        }

        if (!str_cmp(area_arg, "none")) {
            victim->quest_runtime.manual_trigger_area_uid = 0;
            send_to_char("QEdit: manual trigger area cleared.\n\r", ch);
            return;
        }

        if (!str_cmp(area_arg, "here")) {
            if (!victim->in_room || !victim->in_room->area) {
                send_to_char("QEdit: target has no area context.\n\r", ch);
                return;
            }
            victim->quest_runtime.manual_trigger_area_uid = victim->in_room->area->uid;
            printf_to_char(ch, "QEdit: manual trigger area set to %ld (%s).\n\r",
                victim->in_room->area->uid, victim->in_room->area->name);
            return;
        }

        area = NULL;
        if (is_number(area_arg))
            area = get_area_index(atol(area_arg));
        if (!area)
            area = find_area(area_arg);

        if (!area) {
            send_to_char("QEdit: area not found.\n\r", ch);
            return;
        }

        victim->quest_runtime.manual_trigger_area_uid = area->uid;
        printf_to_char(ch, "QEdit: manual trigger area set to %ld (%s).\n\r",
            area->uid, area->name);
        return;
    }

    if (!str_prefix(arg2, "wall")) {
        long value;

        if (IS_NULLSTR(arg3) || !str_cmp(arg3, "none")) {
            victim->quest_runtime.expires_at = 0;
            send_to_char("QEdit: wall expiration cleared.\n\r", ch);
            return;
        }

        if (!is_number(arg3)) {
            send_to_char("QEdit: wall requires numeric minutes offset or unix timestamp.\n\r", ch);
            return;
        }

        value = atol(arg3);
        if (value <= 0) {
            send_to_char("QEdit: wall value must be > 0.\n\r", ch);
            return;
        }

        if (value < 60L * 60L * 24L * 365L)
            victim->quest_runtime.expires_at = now + (value * 60L);
        else
            victim->quest_runtime.expires_at = (time_t)value;

        printf_to_char(ch, "QEdit: wall expiration set to %ld.\n\r",
            (long)victim->quest_runtime.expires_at);
        return;
    }

    if (!str_prefix(arg2, "countdown")) {
        long value;

        if (IS_NULLSTR(arg3) || !str_cmp(arg3, "none")) {
            victim->quest_runtime.expiry_countdown_minutes = 0;
            victim->countdown = 0;
            send_to_char("QEdit: countdown cleared.\n\r", ch);
            return;
        }

        if (!is_number(arg3)) {
            send_to_char("QEdit: countdown requires numeric minutes.\n\r", ch);
            return;
        }

        value = atol(arg3);
        if (value < 0) {
            send_to_char("QEdit: countdown must be >= 0.\n\r", ch);
            return;
        }

        victim->quest_runtime.expiry_countdown_minutes = (int)value;
        victim->countdown = (int)value;
        printf_to_char(ch, "QEdit: countdown set to %ld minute%s.\n\r",
            value, value == 1 ? "" : "s");
        return;
    }

    send_to_char("QEdit: Unknown subcommand. Use SHOW/MODE/AREA/WALL/COUNTDOWN/CLEAR.\n\r", ch);
}

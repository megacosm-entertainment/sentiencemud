/***************************************************************************
 *  channel_moderation.c — Per-channel moderation command surface          *
 *                                                                         *
 *  Staff commands for applying and managing per-channel penalties.        *
 *  Enforcement happens in channel_service_send() via has_channel_penalty. *
 *                                                                         *
 *  Commands:                                                              *
 *    chanmute  <char> [channel|*] <duration> [reason]                    *
 *    chanban   <char> [channel|*] [reason]          (permanent mute)     *
 *    chanwarn  <char> [channel|*] [reason]          (record only)        *
 *    chanunmute <char> [channel|*]                                        *
 *    chanpenalties <char>                                                  *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>

#include "merc.h"
#include "recycle.h"
#include "channel_moderation.h"
#include "account/penalty.h"

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/**
 * chanmod_find_target - Resolve a character name to an online CHAR_DATA.
 *
 * Sends an error to ch if not found.
 *
 * @param ch        Moderator issuing the command
 * @param name      Character name to look up
 * @return          CHAR_DATA pointer, or NULL
 */
static CHAR_DATA *chanmod_find_target(CHAR_DATA *ch, const char *name)
{
    CHAR_DATA *victim;

    if (IS_NULLSTR(name)) {
        send_to_char("Syntax error: character name required.\n\r", ch);
        return NULL;
    }

    victim = get_char_world(ch, (char *)name);
    if (!victim) {
        send_to_char("That character is not online.\n\r", ch);
        return NULL;
    }

    if (IS_NPC(victim)) {
        send_to_char("Channel penalties cannot be applied to NPCs.\n\r", ch);
        return NULL;
    }

    if (!victim->desc || !victim->desc->account) {
        send_to_char("That character has no associated account.\n\r", ch);
        return NULL;
    }

    return victim;
}

/**
 * chanmod_channel_extra - Normalise channel argument for storage in extra.
 *
 * "*" and "all" map to "" (empty = all channels); others passed through as-is.
 *
 * @param arg   Raw channel argument from command line
 * @param buf   Output buffer
 * @param bufsz Output buffer size
 */
static void chanmod_channel_extra(const char *arg, char *buf, size_t bufsz)
{
    buf[0] = '\0';

    if (!IS_NULLSTR(arg) && str_cmp(arg, "*") && str_cmp(arg, "all"))
        strlcpy(buf, arg, bufsz);
}

/**
 * chanmod_channel_label - Human-readable label for a channel extra value.
 *
 * @param extra  Value from penalty->extra (empty = all channels)
 * @return       Display string
 */
static const char *chanmod_channel_label(const char *extra)
{
    return (IS_NULLSTR(extra)) ? "all channels" : extra;
}

/* -------------------------------------------------------------------------
 * do_chanmute
 * ---------------------------------------------------------------------- */

/**
 * do_chanmute - Apply a timed channel mute to a player
 *
 * Syntax: chanmute <char> [channel|*] <duration> [reason]
 *
 *   channel  : channel id (e.g. "gossip") or "*" / "all" for all channels
 *   duration : time string e.g. "30m", "2h", "1d", or "permanent" / "perm"
 *   reason   : optional free-text reason logged with the penalty
 *
 * @param ch        Moderator issuing the command
 * @param argument  Remaining command line
 */
void do_chanmute(CHAR_DATA *ch, char *argument)
{
    char arg_char[MSL];
    char arg_chan[MSL];
    char arg_dur[MSL];
    char channel_extra[64];
    char reason_buf[256];
    CHAR_DATA *victim;
    time_t duration, expires_at;

    argument = one_argument(argument, arg_char);
    if (IS_NULLSTR(arg_char)) {
        send_to_char("Syntax: chanmute <char> [channel|*] <duration> [reason]\n\r", ch);
        send_to_char("  channel : channel id or * for all channels\n\r", ch);
        send_to_char("  duration: 30m, 2h, 1d, or permanent\n\r", ch);
        return;
    }

    victim = chanmod_find_target(ch, arg_char);
    if (!victim)
        return;

    /* Next token: could be channel id or duration */
    argument = one_argument(argument, arg_chan);
    if (IS_NULLSTR(arg_chan)) {
        send_to_char("Syntax: chanmute <char> [channel|*] <duration> [reason]\n\r", ch);
        return;
    }

    /* Peek: if parse_duration fails on arg_chan, treat it as channel + next is duration */
    duration = parse_duration(arg_chan);
    if (duration < 0) {
        /* arg_chan is the channel id; next arg is duration */
        argument = one_argument(argument, arg_dur);
        chanmod_channel_extra(arg_chan, channel_extra, sizeof(channel_extra));
        duration = parse_duration(arg_dur);
        if (duration < 0) {
            send_to_char("ChanMute: invalid duration. Use 30m, 2h, 1d, or permanent.\n\r", ch);
            return;
        }
    } else {
        /* arg_chan was duration; no channel specified = all channels */
        channel_extra[0] = '\0';
        strlcpy(arg_dur, arg_chan, sizeof(arg_dur));
    }

    expires_at = (duration == 0) ? 0 : (current_time + duration);

    /* Remaining text is the reason */
    strlcpy(reason_buf, IS_NULLSTR(argument) ? "(no reason given)" : argument, sizeof(reason_buf));

    add_penalty(victim->desc->account,
                PENALTY_CHAN_MUTE,
                PENALTY_SCOPE_CHARACTER,
                reason_buf,
                ch->name,
                expires_at,
                victim->name,
                channel_extra);

    if (expires_at == 0) {
        act_new("You permanently muted $N on $t.",
                ch, victim, NULL, NULL, NULL, NULL, NULL,
                (void *)chanmod_channel_label(channel_extra), NULL,
                TO_CHAR, POS_DEAD, NULL);
        act_new("You have been permanently muted on $t.",
                ch, victim, NULL, NULL, NULL, NULL, NULL,
                (void *)chanmod_channel_label(channel_extra), NULL,
                TO_VICT, POS_DEAD, NULL);
    } else {
        char dur_buf[64];
        penalty_format_duration(expires_at - current_time, dur_buf, sizeof(dur_buf));
        act_new("You muted $N on $t for $T.",
                ch, victim, NULL, NULL, NULL, NULL, NULL,
                (void *)chanmod_channel_label(channel_extra), dur_buf,
                TO_CHAR, POS_DEAD, NULL);
        act_new("You have been muted on $t for $T.",
                ch, victim, NULL, NULL, NULL, NULL, NULL,
                (void *)chanmod_channel_label(channel_extra), dur_buf,
                TO_VICT, POS_DEAD, NULL);
    }

    log_stringf("ChanMute: %s muted %s on '%s' (reason: %s)",
                ch->name, victim->name,
                chanmod_channel_label(channel_extra), reason_buf);
}

/* -------------------------------------------------------------------------
 * do_chanban
 * ---------------------------------------------------------------------- */

/**
 * do_chanban - Apply a permanent channel mute to a player
 *
 * Shorthand for chanmute with permanent duration.
 * Syntax: chanban <char> [channel|*] [reason]
 *
 * @param ch        Moderator issuing the command
 * @param argument  Remaining command line
 */
void do_chanban(CHAR_DATA *ch, char *argument)
{
    char arg_char[MSL];
    char arg_chan[MSL];
    char channel_extra[64];
    char reason_buf[256];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg_char);
    if (IS_NULLSTR(arg_char)) {
        send_to_char("Syntax: chanban <char> [channel|*] [reason]\n\r", ch);
        send_to_char("  channel : channel id or * for all channels (default: all)\n\r", ch);
        return;
    }

    victim = chanmod_find_target(ch, arg_char);
    if (!victim)
        return;

    /* Next token: optional channel id */
    argument = one_argument(argument, arg_chan);
    if (!IS_NULLSTR(arg_chan) && parse_duration(arg_chan) < 0) {
        /* Not a duration token — treat as channel id */
        chanmod_channel_extra(arg_chan, channel_extra, sizeof(channel_extra));
    } else {
        /* arg_chan was either empty or a duration-like token — treat as reason start */
        channel_extra[0] = '\0';
        if (!IS_NULLSTR(arg_chan)) {
            /* Prepend it back to argument as reason */
            char tmp[MSL];
            snprintf(tmp, sizeof(tmp), "%s %s", arg_chan, IS_NULLSTR(argument) ? "" : argument);
            strlcpy(reason_buf, tmp, sizeof(reason_buf));
            goto apply;
        }
    }

    strlcpy(reason_buf, IS_NULLSTR(argument) ? "(no reason given)" : argument, sizeof(reason_buf));

apply:
    add_penalty(victim->desc->account,
                PENALTY_CHAN_MUTE,
                PENALTY_SCOPE_CHARACTER,
                reason_buf,
                ch->name,
                0 /* permanent */,
                victim->name,
                channel_extra);

    act_new("You permanently banned $N from $t.",
            ch, victim, NULL, NULL, NULL, NULL, NULL,
            (void *)chanmod_channel_label(channel_extra), NULL,
            TO_CHAR, POS_DEAD, NULL);
    act_new("You have been permanently banned from $t.",
            ch, victim, NULL, NULL, NULL, NULL, NULL,
            (void *)chanmod_channel_label(channel_extra), NULL,
            TO_VICT, POS_DEAD, NULL);

    log_stringf("ChanBan: %s banned %s from '%s' (reason: %s)",
                ch->name, victim->name,
                chanmod_channel_label(channel_extra), reason_buf);
}

/* -------------------------------------------------------------------------
 * do_chanwarn
 * ---------------------------------------------------------------------- */

/**
 * do_chanwarn - Record a channel warning against a player (no enforcement)
 *
 * Syntax: chanwarn <char> [channel|*] [reason]
 *
 * @param ch        Moderator issuing the command
 * @param argument  Remaining command line
 */
void do_chanwarn(CHAR_DATA *ch, char *argument)
{
    char arg_char[MSL];
    char arg_chan[MSL];
    char channel_extra[64];
    char reason_buf[256];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg_char);
    if (IS_NULLSTR(arg_char)) {
        send_to_char("Syntax: chanwarn <char> [channel|*] [reason]\n\r", ch);
        return;
    }

    victim = chanmod_find_target(ch, arg_char);
    if (!victim)
        return;

    argument = one_argument(argument, arg_chan);
    if (!IS_NULLSTR(arg_chan) && str_cmp(arg_chan, "*") && str_cmp(arg_chan, "all")
        && parse_duration(arg_chan) < 0) {
        chanmod_channel_extra(arg_chan, channel_extra, sizeof(channel_extra));
    } else {
        channel_extra[0] = '\0';
        /* arg_chan (if any) becomes start of reason */
        if (!IS_NULLSTR(arg_chan)) {
            char tmp[MSL];
            snprintf(tmp, sizeof(tmp), "%s %s", arg_chan, IS_NULLSTR(argument) ? "" : argument);
            strlcpy(reason_buf, tmp, sizeof(reason_buf));
            goto apply_warn;
        }
    }

    strlcpy(reason_buf, IS_NULLSTR(argument) ? "(no reason given)" : argument, sizeof(reason_buf));

apply_warn:
    add_penalty(victim->desc->account,
                PENALTY_CHAN_WARN,
                PENALTY_SCOPE_CHARACTER,
                reason_buf,
                ch->name,
                0 /* permanent record */,
                victim->name,
                channel_extra);

    act_new("You warned $N about their behavior on $t.",
            ch, victim, NULL, NULL, NULL, NULL, NULL,
            (void *)chanmod_channel_label(channel_extra), NULL,
            TO_CHAR, POS_DEAD, NULL);
    act_new("{RWarning:{x You have been warned about your behavior on $t.",
            ch, victim, NULL, NULL, NULL, NULL, NULL,
            (void *)chanmod_channel_label(channel_extra), NULL,
            TO_VICT, POS_DEAD, NULL);

    log_stringf("ChanWarn: %s warned %s on '%s' (reason: %s)",
                ch->name, victim->name,
                chanmod_channel_label(channel_extra), reason_buf);
}

/* -------------------------------------------------------------------------
 * do_chanunmute
 * ---------------------------------------------------------------------- */

/**
 * do_chanunmute - Remove active channel mutes from a player
 *
 * Removes all unexpired PENALTY_CHAN_MUTE entries matching the specified
 * channel (or all channel mutes if channel is "*" / omitted).
 *
 * Syntax: chanunmute <char> [channel|*]
 *
 * @param ch        Moderator issuing the command
 * @param argument  Remaining command line
 */
void do_chanunmute(CHAR_DATA *ch, char *argument)
{
    char arg_char[MSL];
    char arg_chan[MSL];
    char channel_extra[64];
    CHAR_DATA *victim;
    ACCOUNT_DATA *account;
    PENALTY_DATA *p;
    int removed = 0;
    int index;

    argument = one_argument(argument, arg_char);
    if (IS_NULLSTR(arg_char)) {
        send_to_char("Syntax: chanunmute <char> [channel|*]\n\r", ch);
        return;
    }

    victim = chanmod_find_target(ch, arg_char);
    if (!victim)
        return;

    argument = one_argument(argument, arg_chan);
    chanmod_channel_extra(arg_chan, channel_extra, sizeof(channel_extra));

    account = victim->desc->account;

    /* Walk penalties from the end to remove by index safely */
    index = count_penalties(account) - 1;
    while (index >= 0) {
        p = get_penalty_by_index(account, index);
        if (p && p->type == PENALTY_CHAN_MUTE && !is_penalty_expired(p)) {
            /* Match: remove if channel_extra is "" (all) or matches exactly */
            bool match = (channel_extra[0] == '\0')
                         || (!IS_NULLSTR(p->extra) && !str_cmp(p->extra, channel_extra));
            if (match) {
                remove_penalty(account, index);
                removed++;
            }
        }
        index--;
    }

    if (removed > 0) {
        act_new("Removed $T channel mute(s) from $N.",
                ch, victim, NULL, NULL, NULL, NULL, NULL,
                NULL, formatf("%d", removed),
                TO_CHAR, POS_DEAD, NULL);
        act_new("{GYour channel mute has been lifted.{x",
                ch, victim, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL,
                TO_VICT, POS_DEAD, NULL);
        log_stringf("ChanUnmute: %s removed %d mute(s) from %s (channel: %s)",
                    ch->name, removed, victim->name,
                    IS_NULLSTR(channel_extra) ? "*" : channel_extra);
    } else {
        act_new("$N has no active channel mutes matching that filter.",
                ch, victim, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, TO_CHAR, POS_DEAD, NULL);
    }
}

/* -------------------------------------------------------------------------
 * do_chanpenalties
 * ---------------------------------------------------------------------- */

/**
 * do_chanpenalties - List channel penalties for a player
 *
 * Displays all PENALTY_CHAN_MUTE and PENALTY_CHAN_WARN entries for the
 * specified character, including expired ones for audit purposes.
 *
 * Syntax: chanpenalties <char>
 *
 * @param ch        Staff member viewing the list
 * @param argument  Character name
 */
void do_chanpenalties(CHAR_DATA *ch, char *argument)
{
    char arg_char[MSL];
    CHAR_DATA *victim;
    ACCOUNT_DATA *account;
    PENALTY_DATA *p;
    BUFFER *output;
    int count = 0;
    int index = 0;

    argument = one_argument(argument, arg_char);
    if (IS_NULLSTR(arg_char)) {
        send_to_char("Syntax: chanpenalties <char>\n\r", ch);
        return;
    }

    victim = chanmod_find_target(ch, arg_char);
    if (!victim)
        return;

    account = victim->desc->account;
    output = new_buf();

    add_buf(output, formatf("{WChannel penalties for %s:{x\n\r", victim->name));
    add_buf(output, "{D------------------------------------------------------{x\n\r");

    for (p = account->penalties; p; p = p->next) {
        char exp_buf[64];
        bool expired;

        if (p->type != PENALTY_CHAN_MUTE && p->type != PENALTY_CHAN_WARN)
            continue;

        /* Character-scope: only show if matching this character */
        if (p->scope == PENALTY_SCOPE_CHARACTER
            && str_cmp(p->target_name, victim->name))
            continue;

        expired = is_penalty_expired(p);
        count++;

        if (p->expires_at == 0)
            strlcpy(exp_buf, "permanent", sizeof(exp_buf));
        else if (expired)
            strlcpy(exp_buf, "expired", sizeof(exp_buf));
        else
            penalty_format_duration(p->expires_at - current_time, exp_buf, sizeof(exp_buf));

        add_buf(output, formatf("  {Y[%3d]{x %-12s %-16s %-12s %s\n\r",
                ++index,
                penalty_type_name(p->type),
                IS_NULLSTR(p->extra) ? "all channels" : p->extra,
                exp_buf,
                IS_NULLSTR(p->reason) ? "" : p->reason));

        if (!IS_NULLSTR(p->applied_by))
            add_buf(output, formatf("       {Dby %s{x\n\r", p->applied_by));
    }

    if (count == 0)
        add_buf(output, "  (none)\n\r");

    add_buf(output, "{D------------------------------------------------------{x\n\r");
    page_to_char(buf_string(output), ch);
    free_buf(output);
}

#include <string.h>
#include "merc.h"
#include "channel_review.h"
#include "channel_transport.h"

/**
 * do_rview - Staff review queue moderation command scaffold
 *
 * Phase 1 skeleton for staff review queue tooling. Provides command shape
 * and action flow for list/read/ack against the dedicated review stream.
 *
 * @param ch        Character executing the command
 * @param argument  Subcommand and arguments
 */
void do_rview(CHAR_DATA *ch, char *argument)
{
    char arg1[MIL];
    char arg2[MIL];

    if (!IS_IMMORTAL(ch)) {
        send_to_char("You do not have access to that command.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (IS_NULLSTR(arg1) || !str_prefix(arg1, "help")) {
        send_to_char("Syntax: rview list [limit]\n\r", ch);
        send_to_char("        rview read <stream_id>\n\r", ch);
        send_to_char("        rview ack <stream_id> [note]\n\r", ch);
        send_to_char("\n\r", ch);
        send_to_char("Review Stream: " CHANNEL_REVIEW_STREAM "\n\r", ch);
        return;
    }

    if (!str_prefix(arg1, "list")) {
        printf_to_char(ch,
            "rview(list): stream=%s backend=%s\n\r",
            CHANNEL_REVIEW_STREAM,
            channel_transport_backend_name());
        send_to_char("rview scaffold: list/read integration pending fetch-history backend implementation.\n\r", ch);
        return;
    }

    if (!str_prefix(arg1, "read")) {
        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: rview read <stream_id>\n\r", ch);
            return;
        }

        printf_to_char(ch, "rview(read): requested id %s\n\r", arg2);
        send_to_char("rview scaffold: read payload rendering is pending Phase 2 history retrieval.\n\r", ch);
        return;
    }

    if (!str_prefix(arg1, "ack")) {
        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: rview ack <stream_id> [note]\n\r", ch);
            return;
        }

        printf_to_char(ch, "rview(ack): marked %s (local scaffold)\n\r", arg2);
        if (!IS_NULLSTR(argument))
            printf_to_char(ch, "note: %s\n\r", argument);

        send_to_char("rview scaffold: persistent ack state is pending review-state storage design.\n\r", ch);
        return;
    }

    send_to_char("Syntax: rview list|read|ack ...\n\r", ch);
}

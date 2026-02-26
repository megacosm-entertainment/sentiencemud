#include <string.h>
#include "merc.h"
#include "channel_service.h"
#include "mxp_links.h"

/* BUFFER helper prototypes (implemented in mem.c). */
BUFFER *new_buf(void);
void free_buf(BUFFER *buffer);
char *buf_string(BUFFER *buffer);

static bool rview_extract_quoted_field(const char *detail,
                                       const char *key,
                                       char *out,
                                       size_t out_sz)
{
    const char *start;
    const char *cursor;
    size_t used = 0;

    if (!out || out_sz == 0)
        return false;

    out[0] = '\0';

    if (IS_NULLSTR(detail) || IS_NULLSTR(key))
        return false;

    start = strstr(detail, key);
    if (!start)
        return false;

    start += strlen(key);
    cursor = start;

    while (*cursor && *cursor != '"') {
        if (used + 1 < out_sz)
            out[used++] = *cursor;
        cursor++;
    }

    out[used] = '\0';
    return true;
}

static bool rview_extract_unquoted_field(const char *detail,
                                         const char *key,
                                         char *out,
                                         size_t out_sz)
{
    const char *start;
    const char *cursor;
    size_t used = 0;

    if (!out || out_sz == 0)
        return false;

    out[0] = '\0';

    if (IS_NULLSTR(detail) || IS_NULLSTR(key))
        return false;

    start = strstr(detail, key);
    if (!start)
        return false;

    start += strlen(key);
    cursor = start;

    while (*cursor && *cursor != ' ' && *cursor != '\n' && *cursor != '\r') {
        if (used + 1 < out_sz)
            out[used++] = *cursor;
        cursor++;
    }

    out[used] = '\0';
    return true;
}

static void rview_render_player_report_detail(CHAR_DATA *ch,
                                              const char *detail)
{
    char target_id[64];
    char target_sender[64];
    char target_text[1024];
    char notes[1024];
    char context_prev[2048];
    char context_copy[2048];
    char *cursor;
    int context_index = 0;

    if (!ch || IS_NULLSTR(detail))
        return;

    target_id[0] = '\0';
    target_sender[0] = '\0';
    target_text[0] = '\0';
    notes[0] = '\0';
    context_prev[0] = '\0';

    rview_extract_unquoted_field(detail, "target_id=", target_id, sizeof(target_id));
    rview_extract_unquoted_field(detail, "target_sender=", target_sender, sizeof(target_sender));
    rview_extract_quoted_field(detail, "target_text=\"", target_text, sizeof(target_text));
    rview_extract_quoted_field(detail, "notes=\"", notes, sizeof(notes));
    rview_extract_quoted_field(detail, "context_prev=\"", context_prev, sizeof(context_prev));

    if (!IS_NULLSTR(target_text)) {
        printf_to_char(ch,
                       "  Target  : [{W%s{x] {W%s{x: %s\n\r",
                       IS_NULLSTR(target_id) ? "unknown" : target_id,
                       IS_NULLSTR(target_sender) ? "unknown" : target_sender,
                       target_text);
    }

    if (!IS_NULLSTR(notes) && str_cmp(notes, "(none)"))
        printf_to_char(ch, "  Notes   : %s\n\r", notes);

    if (IS_NULLSTR(context_prev)) {
        send_to_char("  Context : (no previous messages captured)\n\r", ch);
        return;
    }

    send_to_char("  Context :\n\r", ch);
    strlcpy(context_copy, context_prev, sizeof(context_copy));
    cursor = context_copy;

    while (*cursor) {
        char *sep = strstr(cursor, " || ");

        if (sep)
            *sep = '\0';

        if (!IS_NULLSTR(cursor)) {
            context_index++;
            printf_to_char(ch, "    %d) %s\n\r", context_index, cursor);
        }

        if (!sep)
            break;

        cursor = sep + 4;
    }

    if (context_index == 0)
        send_to_char("    (no previous messages captured)\n\r", ch);
}

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
    char arg3[MIL];
    char resolved_report_id[64];

    #define RVIEW_REPORT_LINK(_ch,_rid,_out,_outsz) do { \
        BUFFER *mxp_buf = new_buf(); \
        char cmd_read[MSL]; \
        char cmd_ack[MSL]; \
        mxp_cmd_hint_t items[2]; \
        snprintf(cmd_read, sizeof(cmd_read), "rview read %s", (_rid)); \
        snprintf(cmd_ack, sizeof(cmd_ack), "rview ack %s", (_rid)); \
        items[0].cmd = cmd_read; items[0].hint = "View report details"; \
        items[1].cmd = cmd_ack; items[1].hint = "Acknowledge report"; \
        mxp_link_multi((_ch)->desc, mxp_buf, (_rid), items, 2); \
        strlcpy((_out), buf_string(mxp_buf), (_outsz)); \
        free_buf(mxp_buf); \
    } while (0)

    if (!IS_IMMORTAL(ch)) {
        send_to_char("You do not have access to that command.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (IS_NULLSTR(arg1) || !str_prefix(arg1, "help")) {
        send_to_char("Syntax: rview list [limit]\n\r", ch);
        send_to_char("        rview listall [limit]\n\r", ch);
        send_to_char("        rview read <report-id|index>\n\r", ch);
        send_to_char("        rview ack <report-id|index> [note]\n\r", ch);
        send_to_char("        rview purge acked\n\r", ch);
        send_to_char("        rview trim <max-entries>\n\r", ch);
        send_to_char("\n\r", ch);
        send_to_char("Use 'rview list' for pending reports.\n\r", ch);
        return;
    }

    if (!str_prefix(arg1, "list") || !str_prefix(arg1, "listall")) {
        CHANNEL_STAFF_REPORT_ENTRY entries[50];
        int limit = 15;
        int i;
        int count;
        bool include_ack = !str_prefix(arg1, "listall");

        if (!IS_NULLSTR(arg2) && is_number(arg2))
            limit = URANGE(1, atoi(arg2), 50);

        count = channel_service_staff_report_recent(entries,
                                                    UMIN(limit, (int)(sizeof(entries) / sizeof(entries[0]))),
                                                    include_ack);

        printf_to_char(ch,
                       "{YReport queue:{x pending={W%d{x backend={W%s{x\n\r",
                       channel_service_staff_report_count(),
                       channel_service_backend_name());

        if (count <= 0) {
            send_to_char(include_ack
                         ? "No reports in queue.\n\r"
                         : "No pending reports in queue.\n\r",
                         ch);
            return;
        }

        for (i = 0; i < count; i++) {
            char when_buf[32];
            char report_display[MAX_STRING_LENGTH];
            struct tm *tm_info = localtime(&entries[i].timestamp);

            if (tm_info)
                strftime(when_buf, sizeof(when_buf), "%Y-%m-%d %H:%M", tm_info);
            else
                strlcpy(when_buf, "unknown-time", sizeof(when_buf));

            if (ch->desc && isMXP(ch->desc))
                RVIEW_REPORT_LINK(ch, entries[i].report_id, report_display, sizeof(report_display));
            else
                strlcpy(report_display, entries[i].report_id, sizeof(report_display));

            printf_to_char(ch,
                           "{Y#%2d{x [%s] id={W%s{x ch={W%s{x reason={W%s{x by={W%s{x\n\r",
                           i + 1,
                           when_buf,
                           report_display,
                           entries[i].channel_id,
                           entries[i].reason,
                           entries[i].reporter_name);
        }

        send_to_char("Use: rview read <report-id|index>   rview ack <report-id|index>\n\r", ch);
        return;
    }

    if (!str_prefix(arg1, "read")) {
        CHANNEL_STAFF_REPORT_ENTRY entry;
        bool acknowledged = false;

        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: rview read <report-id|index>\n\r", ch);
            return;
        }

        if (is_number(arg2)) {
            int idx = atoi(arg2);
            if (!channel_service_staff_report_by_index(idx, false, &entry, &acknowledged)
                && !channel_service_staff_report_by_index(idx, true, &entry, &acknowledged)) {
                send_to_char("No report exists at that index.\n\r", ch);
                return;
            }
        } else {
            strlcpy(resolved_report_id, arg2, sizeof(resolved_report_id));
            if (!channel_service_staff_report_by_id(resolved_report_id, &entry, &acknowledged)) {
                send_to_char("No report exists for that ID.\n\r", ch);
                return;
            }
        }

        printf_to_char(ch,
                       "{YReport Detail{x\n\r"
                       "  ID      : {W%s{x\n\r"
                       "  Queue   : {W%s{x\n\r"
                       "  Channel : {W%s{x\n\r"
                       "  Reason  : {W%s{x\n\r"
                       "  Reporter: {W%s{x\n\r"
                       "  State   : %s\n\r",
                       entry.report_id,
                       entry.queue_name,
                       entry.channel_id,
                       entry.reason,
                       entry.reporter_name,
                       acknowledged ? "{Gacknowledged{x" : "{Ypending{x");

        if (!IS_NULLSTR(entry.detail_text)) {
            if (strstr(entry.detail_text, "kind=player_report") != NULL)
                rview_render_player_report_detail(ch, entry.detail_text);
            else
                printf_to_char(ch, "  Detail  : %.2000s\n\r", entry.detail_text);
        }
        return;
    }

    if (!str_prefix(arg1, "ack")) {
        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: rview ack <report-id|index> [note]\n\r", ch);
            return;
        }

        if (is_number(arg2)) {
            CHANNEL_STAFF_REPORT_ENTRY entry;
            bool acknowledged = false;
            int idx = atoi(arg2);

            if (!channel_service_staff_report_by_index(idx, false, &entry, &acknowledged)
                && !channel_service_staff_report_by_index(idx, true, &entry, &acknowledged)) {
                send_to_char("No report exists at that index.\n\r", ch);
                return;
            }

            strlcpy(resolved_report_id, entry.report_id, sizeof(resolved_report_id));
        } else {
            strlcpy(resolved_report_id, arg2, sizeof(resolved_report_id));
        }

        if (!channel_service_staff_report_ack(resolved_report_id, ch->name)) {
            send_to_char("No report exists for that ID.\n\r", ch);
            return;
        }

        printf_to_char(ch, "Report %s marked acknowledged.\n\r", resolved_report_id);
        if (!IS_NULLSTR(arg3) || !IS_NULLSTR(argument))
            printf_to_char(ch, "Ack note: %s %s\n\r", arg3, argument);

        return;
    }

    if (!str_prefix(arg1, "purge")) {
        int removed;

        if (str_prefix(arg2, "acked") && str_prefix(arg2, "acknowledged")) {
            send_to_char("Syntax: rview purge acked\n\r", ch);
            return;
        }

        removed = channel_service_staff_report_purge_acknowledged();
        printf_to_char(ch, "Purged %d acknowledged report entries.\n\r", removed);
        return;
    }

    if (!str_prefix(arg1, "trim")) {
        int max_entries;
        int removed;

        if (IS_NULLSTR(arg2) || !is_number(arg2) || atoi(arg2) < 1) {
            send_to_char("Syntax: rview trim <max-entries>\n\r", ch);
            return;
        }

        max_entries = atoi(arg2);
        removed = channel_service_staff_report_trim(max_entries);
        printf_to_char(ch,
                       "Trimmed queue to max %d entries (removed %d).\n\r",
                       max_entries,
                       removed);
        return;
    }

    send_to_char("Syntax: rview list|listall|read|ack|purge|trim ...\n\r", ch);

    #undef RVIEW_REPORT_LINK
}

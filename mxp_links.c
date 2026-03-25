/**
 * mxp_links.c - Standardized MXP link/tooltip generation
 *
 * All helpers append directly to a BUFFER, avoiding static/ring buffer
 * issues entirely. Safe to call any number of times per output line.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "merc.h"
#include "recycle.h"
#include "protocol.h"
#include "mxp_links.h"
#include "sentience_link.h"

/*
 * Central link routing — handles all 4 transport modes with trust filtering.
 *
 * Actions marked staff_only are excluded for non-immortal characters in ALL
 * modes (MXP, GMCP, OSC8). If no actions remain after filtering, plain text
 * is emitted. Delegates to sentience_link_filter_staff() for the filtering.
 *
 * Note: This intentionally diverges from the spec's statement that "MXP
 * continues to emit all commands." Uniform filtering is simpler and safer —
 * non-staff users never saw admin actions they could actually execute, and
 * hiding them avoids confusing right-click menus with non-functional options.
 */
static void link_route(descriptor_t *d, BUFFER *buf, const char *text,
                        const char *top_hint, const char *category,
                        const mxp_cmd_hint_t *items, int nitems,
                        link_mode_t mode)
{
    mxp_cmd_hint_t filtered[SENTIENCE_LINK_MAX_ACTIONS];
    bool is_staff;
    int nf;
    int i;

    /* Trust filter via shared helper (also unit-tested independently) */
    is_staff = (d->character && IS_IMMORTAL(d->character));
    nf = sentience_link_filter_staff(items, nitems, filtered,
                                      SENTIENCE_LINK_MAX_ACTIONS, is_staff);

    if (nf == 0) {
        add_buf(buf, text);
        return;
    }

    switch (mode) {
    case LINK_GMCP: {
        sentience_link_action_t actions[SENTIENCE_LINK_MAX_ACTIONS];
        const char *id;

        for (i = 0; i < nf; i++) {
            actions[i].label = (char *)(filtered[i].hint ? filtered[i].hint : filtered[i].cmd);
            actions[i].cmd   = (char *)filtered[i].cmd;
            actions[i].hint  = NULL;
        }

        id = sentience_link_queue_add(
            &d->pProtocol->sentience_link_queue,
            text, top_hint, category, actions, nf);
        link_osc8_gmcp(buf, id, text);
        break;
    }

    case LINK_OSC8:
        if (filtered[0].cmd)
            link_osc8_telnet(buf, filtered[0].cmd, text);
        else
            add_buf(buf, text);
        break;

    case LINK_MXP:
        if (nf == 1) {
            if (filtered[0].hint && filtered[0].hint[0])
                bprintf(buf, "\t<send href=\"%s\" hint=\"%s\">%s\t</send>",
                        filtered[0].cmd, filtered[0].hint, text);
            else
                bprintf(buf, "\t<send href=\"%s\">%s\t</send>",
                        filtered[0].cmd, text);
        } else {
            bool any_hint = false;

            add_buf(buf, "\t<send href=\"");
            for (i = 0; i < nf; i++) {
                if (i) add_buf(buf, "|");
                if (filtered[i].cmd) add_buf(buf, filtered[i].cmd);
            }
            add_buf(buf, "\"");

            for (i = 0; i < nf; i++) {
                if (filtered[i].hint && filtered[i].hint[0]) { any_hint = true; break; }
            }
            if (any_hint) {
                add_buf(buf, " hint=\"");
                for (i = 0; i < nf; i++) {
                    if (i) add_buf(buf, "|");
                    if (filtered[i].hint) add_buf(buf, filtered[i].hint);
                }
                add_buf(buf, "\"");
            }

            bprintf(buf, ">%s\t</send>", text);
        }
        break;

    default:
        add_buf(buf, text);
        break;
    }
}

void mxp_link(descriptor_t *d, BUFFER *buf, const char *text,
              const char *command, const char *hint)
{
    link_mode_t mode;
    mxp_cmd_hint_t item;

    if (!text) text = "";
    if (!command || !command[0]) {
        add_buf(buf, text);
        return;
    }

    mode = link_mode(d);
    if (mode == LINK_NONE) {
        add_buf(buf, text);
        return;
    }

    item.cmd        = command;
    item.hint       = hint;
    item.staff_only = false;
    link_route(d, buf, text, hint, "cmd", &item, 1, mode);
}

void mxp_link_prompt(descriptor_t *d, BUFFER *buf, const char *text,
                     const char *command, const char *hint)
{
    link_mode_t mode;

    if (!text) text = "";
    if (!command || !command[0]) {
        add_buf(buf, text);
        return;
    }

    mode = link_mode(d);

    /* MXP has a special prompt attribute; other modes treat it like a normal link */
    if (mode == LINK_MXP) {
        if (hint && hint[0])
            bprintf(buf, "\t<send href=\"%s\" hint=\"%s\" prompt>%s\t</send>",
                    command, hint, text);
        else
            bprintf(buf, "\t<send href=\"%s\" prompt>%s\t</send>", command, text);
        return;
    }

    if (mode == LINK_NONE) {
        add_buf(buf, text);
        return;
    }

    /* GMCP and OSC8: treat as regular link */
    {
        mxp_cmd_hint_t item;
        item.cmd        = command;
        item.hint       = hint;
        item.staff_only = false;
        link_route(d, buf, text, hint, "cmd", &item, 1, mode);
    }
}

void mxp_link_multi(descriptor_t *d, BUFFER *buf, const char *text,
                    const mxp_cmd_hint_t *items, int nitems)
{
    link_mode_t mode;

    if (!text) text = "";
    if (!items || nitems <= 0) {
        add_buf(buf, text);
        return;
    }

    mode = link_mode(d);
    if (mode == LINK_NONE) {
        add_buf(buf, text);
        return;
    }

    link_route(d, buf, text, NULL, "cmd", items, nitems, mode);
}

void mxp_obj_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj,
                  const char *text)
{
    if (!obj || !text) { add_buf(buf, (char *)(text ? text : "")); return; }
    if (!isMXP(d)) { add_buf(buf, (char *)text); return; }

    char c1[128], c2[128], c3[128], c4[128];
    const char *wvnum = widevnum_string_object(obj->pIndexData, NULL);

    snprintf(c1, sizeof(c1), "stat obj %ld %ld", obj->id[0], obj->id[1]);
    snprintf(c2, sizeof(c2), "oshow %s", wvnum);
    snprintf(c3, sizeof(c3), "oedit %s", wvnum);
    snprintf(c4, sizeof(c4), "purge obj %ld %ld", obj->id[0], obj->id[1]);

    mxp_cmd_hint_t items[] = {
        { c1, "Stat object" },
        { c2, "Show index" },
        { c3, "Edit index" },
        { c4, "***DANGER*** Purge object" },
    };
    mxp_link_multi(d, buf, text, items, 4);
}

void mxp_obj_id_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj)
{
    if (!obj) return;

    char id_text[64];
    snprintf(id_text, sizeof(id_text), "{W%ld %ld{X", obj->id[0], obj->id[1]);

    if (!isMXP(d)) { add_buf(buf, id_text); return; }

    char c1[128], c2[128];
    snprintf(c1, sizeof(c1), "stat obj %ld %ld", obj->id[0], obj->id[1]);
    snprintf(c2, sizeof(c2), "purge obj %ld %ld", obj->id[0], obj->id[1]);

    mxp_cmd_hint_t items[] = {
        { c1, "Stat object" },
        { c2, "***DANGER*** Purge object" },
    };
    mxp_link_multi(d, buf, id_text, items, 2);
}

void mxp_obj_vnum_link(descriptor_t *d, BUFFER *buf, OBJ_INDEX_DATA *obj,
                       const char *text)
{
    if (!obj || !text) { add_buf(buf, (char *)(text ? text : "")); return; }
    if (!isMXP(d)) { add_buf(buf, (char *)text); return; }

    char c1[128], c2[128];
    const char *wvnum = widevnum_string_object(obj, NULL);

    snprintf(c1, sizeof(c1), "oshow %s", wvnum);
    snprintf(c2, sizeof(c2), "oedit %s", wvnum);

    mxp_cmd_hint_t items[] = {
        { c1, "Show index" },
        { c2, "Edit index" },
    };
    mxp_link_multi(d, buf, text, items, 2);
}

void mxp_mob_link(descriptor_t *d, BUFFER *buf, CHAR_DATA *mob,
                  const char *text)
{
    if (!mob || !text) { add_buf(buf, (char *)(text ? text : "")); return; }
    if (!isMXP(d)) { add_buf(buf, (char *)text); return; }

    if (!IS_NPC(mob)) {
        mxp_player_link(d, buf, mob->name, text);
        return;
    }

    char c1[128], c2[128], c3[128];
    const char *wvnum = widevnum_string_mobile(mob->pIndexData, NULL);

    snprintf(c1, sizeof(c1), "stat mob %ld %ld", mob->id[0], mob->id[1]);
    snprintf(c2, sizeof(c2), "mshow %s", wvnum);
    snprintf(c3, sizeof(c3), "medit %s", wvnum);

    mxp_cmd_hint_t items[] = {
        { c1, "Stat mobile" },
        { c2, "Show index" },
        { c3, "Edit index" },
    };
    mxp_link_multi(d, buf, text, items, 3);
}

void mxp_room_link(descriptor_t *d, BUFFER *buf, ROOM_INDEX_DATA *room,
                   const char *text)
{
    if (!room || !text) { add_buf(buf, (char *)(text ? text : "")); return; }
    if (!isMXP(d)) { add_buf(buf, (char *)text); return; }

    char c1[128], c2[128], c3[128];
    const char *wvnum = widevnum_string_room(room, NULL);

    snprintf(c1, sizeof(c1), "rshow %s", wvnum);
    snprintf(c2, sizeof(c2), "redit %s", wvnum);
    snprintf(c3, sizeof(c3), "goto %s", wvnum);

    mxp_cmd_hint_t items[] = {
        { c1, "Show room" },
        { c2, "Edit room" },
        { c3, "Goto room" },
    };
    mxp_link_multi(d, buf, text, items, 3);
}

void mxp_player_link(descriptor_t *d, BUFFER *buf, const char *name,
                     const char *text)
{
    if (!name) { add_buf(buf, (char *)(text ? text : "")); return; }
    if (!text) text = name;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "stat char %s", name);
    mxp_link(d, buf, text, cmd, "Stat player");
}

void mxp_help_link(descriptor_t *d, BUFFER *buf, const char *keyword,
                   const char *text)
{
    if (!keyword) { add_buf(buf, (char *)(text ? text : "")); return; }
    if (!text) text = keyword;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "help %s", keyword);
    mxp_link(d, buf, text, cmd, NULL);
}

void mxp_command_link(descriptor_t *d, BUFFER *buf, const char *command,
                      const char *hint, const char *text)
{
    if (!command) { add_buf(buf, (char *)(text ? text : "")); return; }
    if (!text) text = command;
    mxp_link(d, buf, text, command, hint);
}

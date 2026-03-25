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

/* Extract the first keyword from a space-separated keyword list.
 * Used to build player-facing commands (e.g., "look sword" from "sword steel"). */
static void first_keyword(const char *name_list, char *dst, size_t dst_size)
{
    size_t i = 0;

    if (!name_list || !dst || dst_size == 0)
        return;

    while (name_list[i] && name_list[i] != ' ' && i < dst_size - 1) {
        dst[i] = name_list[i];
        i++;
    }
    dst[i] = '\0';
}

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
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[8];
    char kw[64], p1[128], p2[128];
    char c1[128], c2[128], c3[128], c4[128];

    if (!obj || !text) { add_buf(buf, (text ? text : "")); return; }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, text); return; }

    first_keyword(obj->name, kw, sizeof(kw));

    /* Player actions */
    if (kw[0]) {
        snprintf(p1, sizeof(p1), "look %s", kw);
        items[n++] = (mxp_cmd_hint_t){ p1, "Look", false };

        snprintf(p2, sizeof(p2), "examine %s", kw);
        items[n++] = (mxp_cmd_hint_t){ p2, "Examine", false };
    }

    /* Staff actions */
    {
        const char *wvnum = widevnum_string_object(obj->pIndexData, NULL);

        snprintf(c1, sizeof(c1), "stat obj %ld %ld", obj->id[0], obj->id[1]);
        items[n++] = (mxp_cmd_hint_t){ c1, "Stat object", true };

        snprintf(c2, sizeof(c2), "oshow %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c2, "Show index", true };

        snprintf(c3, sizeof(c3), "oedit %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c3, "Edit index", true };

        snprintf(c4, sizeof(c4), "purge obj %ld %ld", obj->id[0], obj->id[1]);
        items[n++] = (mxp_cmd_hint_t){ c4, "***DANGER*** Purge object", true };
    }

    link_route(d, buf, text, NULL, "obj", items, n, mode);
}

void mxp_obj_id_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj)
{
    link_mode_t mode;
    char id_text[64];
    int n = 0;
    mxp_cmd_hint_t items[2];
    char c1[128], c2[128];

    if (!obj) return;

    snprintf(id_text, sizeof(id_text), "{W%ld %ld{X", obj->id[0], obj->id[1]);

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, id_text); return; }

    snprintf(c1, sizeof(c1), "stat obj %ld %ld", obj->id[0], obj->id[1]);
    items[n++] = (mxp_cmd_hint_t){ c1, "Stat object", true };

    snprintf(c2, sizeof(c2), "purge obj %ld %ld", obj->id[0], obj->id[1]);
    items[n++] = (mxp_cmd_hint_t){ c2, "***DANGER*** Purge object", true };

    link_route(d, buf, id_text, NULL, "obj", items, n, mode);
}

void mxp_obj_vnum_link(descriptor_t *d, BUFFER *buf, OBJ_INDEX_DATA *obj,
                       const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[2];
    char c1[128], c2[128];
    const char *wvnum;

    if (!obj || !text) { add_buf(buf, (text ? text : "")); return; }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, text); return; }

    wvnum = widevnum_string_object(obj, NULL);

    snprintf(c1, sizeof(c1), "oshow %s", wvnum);
    items[n++] = (mxp_cmd_hint_t){ c1, "Show index", true };

    snprintf(c2, sizeof(c2), "oedit %s", wvnum);
    items[n++] = (mxp_cmd_hint_t){ c2, "Edit index", true };

    link_route(d, buf, text, NULL, "obj", items, n, mode);
}

void mxp_mob_link(descriptor_t *d, BUFFER *buf, CHAR_DATA *mob,
                  const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[6];
    char kw[64], p1[128], p2[128];
    char c1[128], c2[128], c3[128];

    if (!mob || !text) { add_buf(buf, (text ? text : "")); return; }

    if (!IS_NPC(mob)) {
        mxp_player_link(d, buf, mob->name, text);
        return;
    }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, text); return; }

    first_keyword(mob->name, kw, sizeof(kw));

    /* Player actions */
    if (kw[0]) {
        snprintf(p1, sizeof(p1), "look %s", kw);
        items[n++] = (mxp_cmd_hint_t){ p1, "Look", false };

        snprintf(p2, sizeof(p2), "consider %s", kw);
        items[n++] = (mxp_cmd_hint_t){ p2, "Consider", false };
    }

    /* Staff actions */
    {
        const char *wvnum = widevnum_string_mobile(mob->pIndexData, NULL);

        snprintf(c1, sizeof(c1), "stat mob %ld %ld", mob->id[0], mob->id[1]);
        items[n++] = (mxp_cmd_hint_t){ c1, "Stat mobile", true };

        snprintf(c2, sizeof(c2), "mshow %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c2, "Show index", true };

        snprintf(c3, sizeof(c3), "medit %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c3, "Edit index", true };
    }

    link_route(d, buf, text, NULL, "mob", items, n, mode);
}

void mxp_room_link(descriptor_t *d, BUFFER *buf, ROOM_INDEX_DATA *room,
                   const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[4];
    char c1[128], c2[128], c3[128];
    const char *wvnum;

    if (!room || !text) { add_buf(buf, (text ? text : "")); return; }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, text); return; }

    wvnum = widevnum_string_room(room, NULL);

    snprintf(c1, sizeof(c1), "rshow %s", wvnum);
    items[n++] = (mxp_cmd_hint_t){ c1, "Show room", true };

    snprintf(c2, sizeof(c2), "redit %s", wvnum);
    items[n++] = (mxp_cmd_hint_t){ c2, "Edit room", true };

    snprintf(c3, sizeof(c3), "goto %s", wvnum);
    items[n++] = (mxp_cmd_hint_t){ c3, "Goto room", true };

    link_route(d, buf, text, NULL, "room", items, n, mode);
}

void mxp_player_link(descriptor_t *d, BUFFER *buf, const char *name,
                     const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[4];
    char p1[256], p2[256];
    char c1[256];

    if (!name) { add_buf(buf, (text ? text : "")); return; }
    if (!text || !text[0]) text = name;

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, text); return; }

    /* Player actions */
    snprintf(p1, sizeof(p1), "look %s", name);
    items[n++] = (mxp_cmd_hint_t){ p1, "Look", false };

    snprintf(p2, sizeof(p2), "tell %s ", name);
    items[n++] = (mxp_cmd_hint_t){ p2, "Tell", false };

    /* Staff actions */
    snprintf(c1, sizeof(c1), "stat char %s", name);
    items[n++] = (mxp_cmd_hint_t){ c1, "Stat player", true };

    link_route(d, buf, text, NULL, "player", items, n, mode);
}

void mxp_help_link(descriptor_t *d, BUFFER *buf, const char *keyword,
                   const char *text)
{
    link_mode_t mode;
    mxp_cmd_hint_t item;
    char cmd[256];

    if (!keyword) { add_buf(buf, (text ? text : "")); return; }
    if (!text) text = keyword;

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, text); return; }

    snprintf(cmd, sizeof(cmd), "help %s", keyword);

    item.cmd = cmd;
    item.hint = "View help";
    item.staff_only = false;
    link_route(d, buf, text, NULL, "help", &item, 1, mode);
}

void mxp_command_link(descriptor_t *d, BUFFER *buf, const char *command,
                      const char *hint, const char *text)
{
    if (!command) { add_buf(buf, (text ? text : "")); return; }
    if (!text) text = command;
    mxp_link(d, buf, text, command, hint);
}

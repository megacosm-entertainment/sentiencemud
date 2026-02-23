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

void mxp_link(descriptor_t *d, BUFFER *buf, const char *text,
              const char *command, const char *hint)
{
    if (!text) text = "";
    if (!command || !command[0] || !isMXP(d)) {
        add_buf(buf, (char *)text);
        return;
    }

    if (hint && hint[0])
        bprintf(buf, "\t<send href=\"%s\" hint=\"%s\">%s\t</send>",
                command, hint, text);
    else
        bprintf(buf, "\t<send href=\"%s\">%s\t</send>", command, text);
}

void mxp_link_prompt(descriptor_t *d, BUFFER *buf, const char *text,
                     const char *command, const char *hint)
{
    if (!text) text = "";
    if (!command || !command[0] || !isMXP(d)) {
        add_buf(buf, (char *)text);
        return;
    }

    if (hint && hint[0])
        bprintf(buf, "\t<send href=\"%s\" hint=\"%s\" prompt>%s\t</send>",
                command, hint, text);
    else
        bprintf(buf, "\t<send href=\"%s\" prompt>%s\t</send>", command, text);
}

void mxp_link_multi(descriptor_t *d, BUFFER *buf, const char *text,
                    const mxp_cmd_hint_t *items, int nitems)
{
    if (!text) text = "";
    if (!items || nitems <= 0 || !isMXP(d)) {
        add_buf(buf, (char *)text);
        return;
    }

    add_buf(buf, (char *)"\t<send href=\"");
    for (int i = 0; i < nitems; i++) {
        if (i) add_buf(buf, (char *)"|");
        if (items[i].cmd)
            add_buf(buf, (char *)items[i].cmd);
    }
    add_buf(buf, (char *)"\"");

    bool any_hint = false;
    for (int i = 0; i < nitems; i++) {
        if (items[i].hint && items[i].hint[0]) {
            any_hint = true;
            break;
        }
    }

    if (any_hint) {
        add_buf(buf, (char *)" hint=\"");
        for (int i = 0; i < nitems; i++) {
            if (i) add_buf(buf, (char *)"|");
            if (items[i].hint)
                add_buf(buf, (char *)items[i].hint);
        }
        add_buf(buf, (char *)"\"");
    }

    bprintf(buf, ">%s\t</send>", text);
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

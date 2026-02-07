/**
 * mxp_links.c - Standardized MXP link/tooltip generation
 *
 * Provides helpers for building MXP <send> tags. Uses a dedicated ring
 * buffer pool (separate from formatf/MXPBuildTag) so multiple mxp_*
 * calls can safely appear in a single sprintf.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "merc.h"
#include "protocol.h"
#include "mxp_links.h"

#define MXP_LINK_BUFS    32
#define MXP_LINK_BUFSZ   4096

static char s_mxp_buf[MXP_LINK_BUFS][MXP_LINK_BUFSZ];
static int  s_mxp_buf_i;

static char *mxp_alloc_buf(void)
{
    char *b = s_mxp_buf[s_mxp_buf_i++ % MXP_LINK_BUFS];
    b[0] = '\0';
    return b;
}

const char *mxp_link(descriptor_t *d, const char *text,
                     const char *command, const char *hint)
{
    if (!text) text = "";
    if (!command || !command[0] || !isMXP(d))
        return text;

    char *out = mxp_alloc_buf();
    if (hint && hint[0])
        snprintf(out, MXP_LINK_BUFSZ,
                 "\t<send href=\"%s\" hint=\"%s\">%s\t</send>",
                 command, hint, text);
    else
        snprintf(out, MXP_LINK_BUFSZ,
                 "\t<send href=\"%s\">%s\t</send>",
                 command, text);
    return out;
}

const char *mxp_link_multi(descriptor_t *d, const char *text,
                           const mxp_cmd_hint_t *items, int nitems)
{
    if (!text) text = "";
    if (!items || nitems <= 0 || !isMXP(d))
        return text;

    char *out = mxp_alloc_buf();
    size_t p = 0;

    p += (size_t)snprintf(out + p, MXP_LINK_BUFSZ - p, "\t<send href=\"");

    for (int i = 0; i < nitems; i++) {
        if (i) out[p++] = '|';
        if (items[i].cmd)
            p += (size_t)snprintf(out + p, MXP_LINK_BUFSZ - p, "%s",
                                  items[i].cmd);
    }

    p += (size_t)snprintf(out + p, MXP_LINK_BUFSZ - p, "\"");

    bool any_hint = false;
    for (int i = 0; i < nitems; i++) {
        if (items[i].hint && items[i].hint[0]) {
            any_hint = true;
            break;
        }
    }

    if (any_hint) {
        p += (size_t)snprintf(out + p, MXP_LINK_BUFSZ - p, " hint=\"");
        for (int i = 0; i < nitems; i++) {
            if (i) out[p++] = '|';
            if (items[i].hint)
                p += (size_t)snprintf(out + p, MXP_LINK_BUFSZ - p, "%s",
                                      items[i].hint);
        }
        p += (size_t)snprintf(out + p, MXP_LINK_BUFSZ - p, "\"");
    }

    snprintf(out + p, MXP_LINK_BUFSZ - p, ">%s\t</send>", text);
    return out;
}

const char *mxp_obj_link(descriptor_t *d, OBJ_DATA *obj, const char *text)
{
    if (!obj || !text) return text ? text : "";
    if (!isMXP(d)) return text;

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
    return mxp_link_multi(d, text, items, 4);
}

const char *mxp_obj_id_link(descriptor_t *d, OBJ_DATA *obj)
{
    if (!obj) return "";

    char id_text[64];
    snprintf(id_text, sizeof(id_text), "{W%ld %ld{X", obj->id[0], obj->id[1]);

    if (!isMXP(d)) return formatf("%s", id_text);

    char c1[128], c2[128];
    snprintf(c1, sizeof(c1), "stat obj %ld %ld", obj->id[0], obj->id[1]);
    snprintf(c2, sizeof(c2), "purge obj %ld %ld", obj->id[0], obj->id[1]);

    mxp_cmd_hint_t items[] = {
        { c1, "Stat object" },
        { c2, "***DANGER*** Purge object" },
    };
    return mxp_link_multi(d, id_text, items, 2);
}

const char *mxp_obj_vnum_link(descriptor_t *d, OBJ_INDEX_DATA *obj,
                              const char *text)
{
    if (!obj || !text) return text ? text : "";
    if (!isMXP(d)) return text;

    char c1[128], c2[128];
    const char *wvnum = widevnum_string_object(obj, NULL);

    snprintf(c1, sizeof(c1), "oshow %s", wvnum);
    snprintf(c2, sizeof(c2), "oedit %s", wvnum);

    mxp_cmd_hint_t items[] = {
        { c1, "Show index" },
        { c2, "Edit index" },
    };
    return mxp_link_multi(d, text, items, 2);
}

const char *mxp_mob_link(descriptor_t *d, CHAR_DATA *mob, const char *text)
{
    if (!mob || !text) return text ? text : "";
    if (!isMXP(d)) return text;

    if (!IS_NPC(mob))
        return mxp_player_link(d, mob->name, text);

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
    return mxp_link_multi(d, text, items, 3);
}

const char *mxp_room_link(descriptor_t *d, ROOM_INDEX_DATA *room,
                          const char *text)
{
    if (!room || !text) return text ? text : "";
    if (!isMXP(d)) return text;

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
    return mxp_link_multi(d, text, items, 3);
}

const char *mxp_player_link(descriptor_t *d, const char *name,
                            const char *text)
{
    if (!name) return text ? text : "";
    if (!text) text = name;
    if (!isMXP(d)) return text;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "stat char %s", name);
    return mxp_link(d, text, cmd, "Stat player");
}

const char *mxp_help_link(descriptor_t *d, const char *keyword,
                          const char *text)
{
    if (!keyword) return text ? text : "";
    if (!text) text = keyword;
    if (!isMXP(d)) return text;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "help %s", keyword);
    return mxp_link(d, text, cmd, NULL);
}

const char *mxp_command_link(descriptor_t *d, const char *command,
                             const char *hint, const char *text)
{
    if (!command) return text ? text : "";
    if (!text) text = command;
    return mxp_link(d, text, command, hint);
}

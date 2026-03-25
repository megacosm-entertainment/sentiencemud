/*
 * sentience_link.c — Link abstraction layer implementation
 *
 * Provides link queue management, JSON building, OSC 8 output,
 * and link mode detection for the 4-way routing in mxp_links.c.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "merc.h"
#include "recycle.h"
#include "protocol.h"
#include "connection.h"
#include "sentience_link.h"

/* ── Stub implementations — filled in by subsequent tasks ── */

link_mode_t link_mode(descriptor_t *d)
{
    (void)d;
    return LINK_NONE;
}

bool is_websocket_connection(descriptor_t *d)
{
    return d && d->conn && d->conn->type == CONN_TYPE_WEBSOCKET_TLS;
}

bool has_osc8_support(descriptor_t *d)
{
    (void)d;
    return false;
}

void sentience_link_queue_init(sentience_link_queue_t *queue)
{
    if (!queue) return;
    memset(queue, 0, sizeof(*queue));
}

void sentience_link_queue_free(sentience_link_queue_t *queue)
{
    if (!queue) return;
    free(queue->entries);
    memset(queue, 0, sizeof(*queue));
}

const char *sentience_link_queue_add(sentience_link_queue_t *queue,
                                      const char *text,
                                      const char *hint,
                                      const char *category,
                                      const sentience_link_action_t *actions,
                                      int num_actions)
{
    (void)queue; (void)text; (void)hint; (void)category;
    (void)actions; (void)num_actions;
    return NULL;
}

json_t *sentience_link_queue_to_json(const sentience_link_queue_t *queue)
{
    (void)queue;
    return json_array();
}

void sentience_link_queue_flush(descriptor_t *d)
{
    (void)d;
}

void link_osc8_gmcp(BUFFER *buf, const char *link_id, const char *text)
{
    if (!buf || !text)
        return;

    if (!link_id || !link_id[0]) {
        add_buf(buf, text);
        return;
    }

    bprintf(buf, "\x1b]8;;%s\x07%s\x1b]8;;\x07", link_id, text);
}

void link_osc8_telnet(BUFFER *buf, const char *primary_cmd, const char *text)
{
    char encoded[512];

    if (!buf || !text)
        return;

    if (!primary_cmd || !primary_cmd[0]) {
        add_buf(buf, text);
        return;
    }

    link_url_encode(encoded, sizeof(encoded), primary_cmd);
    bprintf(buf, "\x1b]8;;mud://%s\x07%s\x1b]8;;\x07", encoded, text);
}

int link_url_encode(char *dst, size_t dst_size, const char *src)
{
    size_t di = 0;

    if (!dst || !src || dst_size == 0)
        return 0;

    for (const char *s = src; *s && di + 3 < dst_size; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[di++] = (char)c;
        } else {
            if (di + 3 >= dst_size) break;  /* Need space for %XX */
            dst[di++] = '%';
            dst[di++] = "0123456789ABCDEF"[(c >> 4) & 0xF];
            dst[di++] = "0123456789ABCDEF"[c & 0xF];
        }
    }

    dst[di] = '\0';
    return (int)di;
}

int sentience_link_filter_staff(const struct mxp_cmd_hint *items, int nitems,
                                 struct mxp_cmd_hint *out, int out_max,
                                 bool is_staff)
{
    (void)items; (void)nitems; (void)out; (void)out_max; (void)is_staff;
    return 0;
}

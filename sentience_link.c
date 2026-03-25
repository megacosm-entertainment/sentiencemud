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
    if (buf && text) add_buf(buf, text);
    (void)link_id;
}

void link_osc8_telnet(BUFFER *buf, const char *primary_cmd, const char *text)
{
    if (buf && text) add_buf(buf, text);
    (void)primary_cmd;
}

int link_url_encode(char *dst, size_t dst_size, const char *src)
{
    (void)dst; (void)dst_size; (void)src;
    if (dst && dst_size > 0) dst[0] = '\0';
    return 0;
}

int sentience_link_filter_staff(const struct mxp_cmd_hint *items, int nitems,
                                 struct mxp_cmd_hint *out, int out_max,
                                 bool is_staff)
{
    (void)items; (void)nitems; (void)out; (void)out_max; (void)is_staff;
    return 0;
}

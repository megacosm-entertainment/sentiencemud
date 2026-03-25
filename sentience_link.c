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
#include "mxp_links.h"
#include "sentience_link.h"

/* Terminals known to support OSC 8 hyperlinks (case-insensitive prefix match) */
static const char *osc8_terminals[] = {
    "mudlet",
    "xterm-256color",
    "xterm-kitty",
    "tmux-256color",
    NULL
};

link_mode_t link_mode(descriptor_t *d)
{
    CHAR_DATA *ch;

    if (!d)
        return LINK_NONE;

    ch = d->character;

    /* Respect player preference for ALL connection types */
    if (ch && !IS_SET(ch->comm, COMM_LINKS))
        return LINK_NONE;

    /* WebSocket uses GMCP links (pre-login: no character, links enabled by default) */
    if (is_websocket_connection(d))
        return LINK_GMCP;

    /* No character yet on non-WebSocket — can't check preference */
    if (!ch)
        return LINK_NONE;

    /* MXP negotiated takes priority (established clients) */
    if (isMXP(d))
        return LINK_MXP;

    /* TTYPE indicates OSC 8 support */
    if (has_osc8_support(d))
        return LINK_OSC8;

    return LINK_NONE;
}

bool is_websocket_connection(descriptor_t *d)
{
    return d && d->conn && d->conn->type == CONN_TYPE_WEBSOCKET_TLS;
}

bool has_osc8_support(descriptor_t *d)
{
    const char *ttype;
    int i;

    if (!d || !d->pProtocol || !d->pProtocol->pLastTTYPE)
        return false;

    ttype = d->pProtocol->pLastTTYPE;

    for (i = 0; osc8_terminals[i]; i++) {
        if (!str_prefix(osc8_terminals[i], ttype))
            return true;
    }

    return false;
}

static void link_entry_clear(sentience_link_entry_t *entry)
{
    int i;

    if (!entry)
        return;

    free(entry->text);
    free(entry->hint);
    free(entry->category);
    for (i = 0; i < entry->num_actions; i++) {
        free(entry->actions[i].label);
        free(entry->actions[i].cmd);
        free(entry->actions[i].hint);
    }
    memset(entry, 0, sizeof(*entry));
}

void sentience_link_queue_init(sentience_link_queue_t *queue)
{
    if (!queue)
        return;

    queue->count    = 0;
    queue->capacity = SENTIENCE_LINK_QUEUE_INITIAL;
    queue->entries  = calloc(queue->capacity, sizeof(sentience_link_entry_t));
}

void sentience_link_queue_free(sentience_link_queue_t *queue)
{
    int i;

    if (!queue || !queue->entries)
        return;

    for (i = 0; i < queue->count; i++)
        link_entry_clear(&queue->entries[i]);

    free(queue->entries);
    queue->entries  = NULL;
    queue->count    = 0;
    queue->capacity = 0;
}

const char *sentience_link_queue_add(sentience_link_queue_t *queue,
                                      const char *text,
                                      const char *hint,
                                      const char *category,
                                      const sentience_link_action_t *actions,
                                      int num_actions)
{
    sentience_link_entry_t *entry;
    int i, nact;

    if (!queue || !queue->entries || queue->count >= SENTIENCE_LINK_QUEUE_MAX)
        return NULL;

    /* Grow if needed */
    if (queue->count >= queue->capacity) {
        int new_cap = queue->capacity * 2;
        sentience_link_entry_t *grown;

        if (new_cap > SENTIENCE_LINK_QUEUE_MAX)
            new_cap = SENTIENCE_LINK_QUEUE_MAX;

        grown = realloc(queue->entries, new_cap * sizeof(sentience_link_entry_t));
        if (!grown)
            return NULL;

        memset(grown + queue->capacity, 0,
               (new_cap - queue->capacity) * sizeof(sentience_link_entry_t));
        queue->entries  = grown;
        queue->capacity = new_cap;
    }

    entry = &queue->entries[queue->count];
    snprintf(entry->id, sizeof(entry->id), "lk_%d", queue->count);
    entry->text     = strdup(text ? text : "");
    entry->hint     = hint ? strdup(hint) : NULL;
    entry->category = strdup(category ? category : "cmd");

    nact = (num_actions > SENTIENCE_LINK_MAX_ACTIONS)
        ? SENTIENCE_LINK_MAX_ACTIONS : num_actions;
    entry->num_actions = nact;
    for (i = 0; i < nact; i++) {
        entry->actions[i].label = strdup(actions[i].label ? actions[i].label : "");
        entry->actions[i].cmd   = strdup(actions[i].cmd ? actions[i].cmd : "");
        entry->actions[i].hint  = actions[i].hint ? strdup(actions[i].hint) : NULL;
    }

    queue->count++;
    return entry->id;
}

json_t *sentience_link_queue_to_json(const sentience_link_queue_t *queue)
{
    json_t *arr = json_array();
    int i, j;

    if (!arr || !queue)
        return arr;

    for (i = 0; i < queue->count; i++) {
        const sentience_link_entry_t *e = &queue->entries[i];
        json_t *link = json_object();
        json_t *actions = json_array();

        json_object_set_new(link, "id",       json_string(e->id));
        json_object_set_new(link, "text",     json_string(e->text ? e->text : ""));
        if (e->hint)
            json_object_set_new(link, "hint", json_string(e->hint));
        json_object_set_new(link, "category", json_string(e->category ? e->category : "cmd"));

        for (j = 0; j < e->num_actions; j++) {
            json_t *act = json_object();
            json_object_set_new(act, "label", json_string(e->actions[j].label ? e->actions[j].label : ""));
            json_object_set_new(act, "cmd",   json_string(e->actions[j].cmd ? e->actions[j].cmd : ""));
            if (e->actions[j].hint)
                json_object_set_new(act, "hint", json_string(e->actions[j].hint));
            json_array_append_new(actions, act);
        }

        json_object_set_new(link, "actions", actions);
        json_array_append_new(arr, link);
    }

    return arr;
}

void sentience_link_queue_flush(descriptor_t *d)
{
    sentience_link_queue_t *queue;
    json_t *arr;
    char *dump;
    int i;

    if (!d || !d->pProtocol)
        return;

    queue = &d->pProtocol->sentience_link_queue;
    if (queue->count == 0)
        return;

    /* Only send GMCP for WebSocket clients with GMCP support */
    if (is_websocket_connection(d) && d->pProtocol->bGMCP) {
        arr = sentience_link_queue_to_json(queue);
        if (arr) {
            dump = json_dumps(arr, JSON_COMPACT);
            if (dump) {
                /*
                 * Send GMCP as a separate WebSocket frame BEFORE the text
                 * frame, so the client has link metadata before it sees
                 * the OSC 8 markers in the text.  Write directly to the
                 * socket via connection_write() to avoid mixing with the
                 * pending text in d->outbuf.
                 */
                char *msg;
                int len;
                size_t msg_size = strlen("Sentience.Link.List") + 1
                                + strlen(dump) + 3; /* " \n\r\0" */
                msg = alloc_mem(msg_size);
                len = snprintf(msg, msg_size, "Sentience.Link.List %s\n\r", dump);
                if (len > 0 && d->conn) {
                    int bytes_written;
                    connection_write(d->conn, msg, len, &bytes_written);
                }
                free_mem(msg, msg_size);
                free(dump);
            }
            json_decref(arr);
        }
    }

    /* Clear queue entries */
    for (i = 0; i < queue->count; i++)
        link_entry_clear(&queue->entries[i]);
    queue->count = 0;
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
    int nf = 0;
    int i;

    if (!items || !out || out_max <= 0)
        return 0;

    for (i = 0; i < nitems && nf < out_max; i++) {
        if (items[i].staff_only && !is_staff)
            continue;
        out[nf++] = items[i];
    }

    return nf;
}

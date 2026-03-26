/*
 * sentience_link.h — Link abstraction layer for GMCP, OSC 8, and MXP
 *
 * Provides transport-agnostic link routing: WebSocket clients get GMCP
 * link metadata + OSC 8 text markers, modern terminals get OSC 8
 * hyperlinks, MXP clients get <send> tags, plain clients get text.
 *
 * See docs/superpowers/specs/2026-03-24-sentience-link-design.md
 */

#ifndef SENTIENCE_LINK_H
#define SENTIENCE_LINK_H

#include <stdbool.h>
#include <jansson.h>

/* Forward declarations */
typedef struct descriptor_data descriptor_t;
typedef struct buf_type BUFFER;

/* Link delivery modes — priority order for routing */
typedef enum {
    LINK_NONE,   /* Plain text, no links */
    LINK_MXP,    /* Telnet MXP <send> tags */
    LINK_OSC8,   /* Telnet OSC 8 hyperlinks (primary action only) */
    LINK_GMCP    /* WebSocket: GMCP metadata + OSC 8 markers in text */
} link_mode_t;

#define SENTIENCE_LINK_MAX_ACTIONS   8
#define SENTIENCE_LINK_QUEUE_MAX     256
#define SENTIENCE_LINK_QUEUE_INITIAL 16

/* A single action in a link context menu */
typedef struct {
    char *label;    /* Menu item text (shown in context menu) */
    char *cmd;      /* MUD command to execute */
    char *hint;     /* Menu item tooltip (may be NULL) */
} sentience_link_action_t;

/* A queued link entry for Sentience.Link.List GMCP */
typedef struct {
    char id[16];    /* "lk_0", "lk_1", ... */
    char *text;     /* Display text */
    char *hint;     /* Link hover tooltip (may be NULL) */
    char *category; /* "obj", "mob", "room", "player", "help", "cmd" */
    int num_actions;
    sentience_link_action_t actions[SENTIENCE_LINK_MAX_ACTIONS];
} sentience_link_entry_t;

/* Link queue — stored on protocol_t, flushed before socket write */
typedef struct {
    int count;
    int capacity;
    sentience_link_entry_t *entries;
} sentience_link_queue_t;

/* ── Routing ───────────────────────────────────────────── */

link_mode_t link_mode(descriptor_t *d);
bool is_websocket_connection(descriptor_t *d);
bool has_osc8_support(descriptor_t *d);

/* ── Queue operations ──────────────────────────────────── */

void sentience_link_queue_init(sentience_link_queue_t *queue);
void sentience_link_queue_free(sentience_link_queue_t *queue);

const char *sentience_link_queue_add(sentience_link_queue_t *queue,
                                      const char *text,
                                      const char *hint,
                                      const char *category,
                                      const sentience_link_action_t *actions,
                                      int num_actions);

json_t *sentience_link_queue_to_json(const sentience_link_queue_t *queue);
void sentience_link_queue_flush(descriptor_t *d);

/* ── Trust filtering ───────────────────────────────────── */

/* Forward declaration — mxp_cmd_hint_t is defined in mxp_links.h */
struct mxp_cmd_hint;

/*
 * Filter an action list by trust level. Copies items where
 * staff_only is false (or is_staff is true) into out[].
 * Returns the number of items written.
 */
int sentience_link_filter_staff(const struct mxp_cmd_hint *items, int nitems,
                                 struct mxp_cmd_hint *out, int out_max,
                                 bool is_staff);

/* ── OSC 8 output helpers ──────────────────────────────── */

void link_osc8_gmcp(BUFFER *buf, const char *link_id, const char *text);
void link_osc8_telnet(BUFFER *buf, const char *primary_cmd, const char *text);
int  link_url_encode(char *dst, size_t dst_size, const char *src);

#endif /* SENTIENCE_LINK_H */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../merc.h"
#include "../protocol.h"
#include "channel_gmcp.h"
#include "channel_registry.h"

extern char *nocolour(const char *string);

/*
 * json_escape_str - Copy src into dst with JSON string escaping
 *
 * Escapes double-quotes and backslashes; strips ASCII control characters.
 * Writes at most dsz-1 chars plus a NUL terminator.
 *
 * @param src  Source string
 * @param dst  Destination buffer
 * @param dsz  Size of destination buffer (including NUL)
 */
static void json_escape_str(const char *src, char *dst, size_t dsz)
{
    size_t i = 0, j = 0;

    while (src[i] && j + 3 < dsz) {
        if (src[i] == '"') {
            dst[j++] = '\\';
            dst[j++] = '"';
        } else if (src[i] == '\\') {
            dst[j++] = '\\';
            dst[j++] = '\\';
        } else if ((unsigned char)src[i] < 0x20) {
            /* skip control characters */
        } else {
            dst[j++] = src[i];
        }
        i++;
    }
    dst[j] = '\0';
}

/**
 * channel_gmcp_recipient_in_scope - Check if recipient is within channel scope of sender
 *
 * @param def        Channel definition (for scope type)
 * @param sender     Sending character (scope context origin)
 * @param recipient  Candidate recipient to check
 * @return true if recipient should receive the GMCP message
 */
static bool channel_gmcp_recipient_in_scope(const CHANNEL_DEF_DATA *def,
                                            CHAR_DATA *sender,
                                            CHAR_DATA *recipient)
{
    if (!def || !sender || !recipient)
        return false;

    switch (def->scope) {
    case CHANNEL_SCOPE_GLOBAL:
        return true;

    case CHANNEL_SCOPE_ROOM_WV:
        return (sender->in_room && recipient->in_room
                && sender->in_room == recipient->in_room);

    case CHANNEL_SCOPE_AREA:
        return (sender->in_room && sender->in_room->area
                && recipient->in_room && recipient->in_room->area
                && sender->in_room->area == recipient->in_room->area);

    case CHANNEL_SCOPE_REGION:
        if (!sender->in_room || !recipient->in_room)
            return false;
        {
            AREA_REGION *sr = get_room_region(sender->in_room);
            AREA_REGION *rr = get_room_region(recipient->in_room);
            return (sr && rr && sr == rr);
        }

    case CHANNEL_SCOPE_GROUP_ID:
        return (IS_VALID(sender->group) && IS_VALID(recipient->group)
                && sender->group == recipient->group);

    case CHANNEL_SCOPE_CHURCH_ID:
        return (sender->church && recipient->church
                && sender->church == recipient->church);

    case CHANNEL_SCOPE_INSTANCE_ID:
        if (!sender->in_room || !recipient->in_room)
            return false;
        {
            INSTANCE *si = get_room_instance(sender->in_room);
            INSTANCE *ri = get_room_instance(recipient->in_room);
            return (si && ri && si == ri);
        }

    case CHANNEL_SCOPE_DUNGEON_ID:
        if (!sender->in_room || !recipient->in_room)
            return false;
        {
            DUNGEON *sd = get_room_dungeon(sender->in_room);
            DUNGEON *rd = get_room_dungeon(recipient->in_room);
            return (sd && rd && sd == rd);
        }

    case CHANNEL_SCOPE_DIRECT_ENTITY:
        /* Direct entity channels use channel_gmcp_send_directed() instead */
        return false;

    default:
        return true;
    }
}

/**
 * channel_gmcp_broadcast - Fire Sentience.Channel.Message to GMCP-capable descriptors
 *
 * Sends a GMCP event to descriptors that have GMCP negotiated and are within
 * the channel's scope relative to the sender.
 *
 * @param def          Channel definition (for scope filtering)
 * @param sender       Sending character (for scope context)
 * @param plain_text   Message text (may contain internal colour codes)
 * @param timestamp    Unix timestamp for the message
 */
void channel_gmcp_broadcast(const CHANNEL_DEF_DATA *def, CHAR_DATA *sender,
                            const char *plain_text, time_t timestamp)
{
    DESCRIPTOR_DATA *d;
    char stripped[MSL];
    char esc_channel[MSL];
    char esc_sender[MSL];
    char esc_text[MSL];
    char json_body[MSL + 128];
    const char *nc;

    if (!def || !sender || !plain_text)
        return;

    nc = nocolour(plain_text);
    strncpy(stripped, nc ? nc : plain_text, sizeof(stripped) - 1);
    stripped[sizeof(stripped) - 1] = '\0';

    json_escape_str(def->id,        esc_channel, sizeof(esc_channel));
    json_escape_str(sender->name,   esc_sender,  sizeof(esc_sender));
    json_escape_str(stripped,        esc_text,    sizeof(esc_text));

    snprintf(json_body, sizeof(json_body),
             "{\"channel\":\"%s\",\"sender\":\"%s\",\"text\":\"%s\",\"timestamp\":%ld}",
             esc_channel, esc_sender, esc_text, (long)timestamp);

    for (d = descriptor_list; d != NULL; d = d->next) {
        if (!d->character)
            continue;
        if (!d->pProtocol || !d->pProtocol->bGMCP)
            continue;
        if (!channel_gmcp_recipient_in_scope(def, sender, d->character))
            continue;
        SendGMCPRaw(d, "Sentience.Channel.Message", json_body);
    }
}

/**
 * channel_gmcp_send_directed - Fire Sentience.Channel.Message to sender and recipient only
 *
 * Sends a GMCP event to the sender's descriptor and the recipient's descriptor
 * (when each has GMCP negotiated).  Adds a "tell_target" field to the JSON body.
 *
 * @param sender       Sending character
 * @param recipient    Receiving character
 * @param channel_id   Channel identifier string (e.g. "tell")
 * @param plain_text   Message text (may contain internal colour codes)
 * @param timestamp    Unix timestamp for the message
 */
void channel_gmcp_send_directed(CHAR_DATA *sender, CHAR_DATA *recipient,
                                const char *channel_id, const char *plain_text,
                                time_t timestamp)
{
    DESCRIPTOR_DATA *d;
    char stripped[MSL];
    char esc_channel[MSL];
    char esc_sender[MSL];
    char esc_text[MSL];
    char esc_target[MSL];
    char json_body[MSL + 256];
    const char *nc;

    if (!sender || !recipient || !channel_id || !plain_text)
        return;

    nc = nocolour(plain_text);
    strncpy(stripped, nc ? nc : plain_text, sizeof(stripped) - 1);
    stripped[sizeof(stripped) - 1] = '\0';

    json_escape_str(channel_id,      esc_channel, sizeof(esc_channel));
    json_escape_str(sender->name,    esc_sender,  sizeof(esc_sender));
    json_escape_str(stripped,        esc_text,    sizeof(esc_text));
    json_escape_str(recipient->name, esc_target,  sizeof(esc_target));

    snprintf(json_body, sizeof(json_body),
             "{\"channel\":\"%s\",\"sender\":\"%s\",\"text\":\"%s\",\"timestamp\":%ld,\"tell_target\":\"%s\"}",
             esc_channel, esc_sender, esc_text, (long)timestamp, esc_target);

    for (d = descriptor_list; d != NULL; d = d->next) {
        if (!d->character)
            continue;
        if (!d->pProtocol || !d->pProtocol->bGMCP)
            continue;
        if (d->character != sender && d->character != recipient)
            continue;
        SendGMCPRaw(d, "Sentience.Channel.Message", json_body);
    }
}

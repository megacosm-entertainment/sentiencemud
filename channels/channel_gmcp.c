#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../merc.h"
#include "../protocol.h"
#include "channel_gmcp.h"

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
 * channel_gmcp_broadcast - Fire Sentience.Channel.Message to all GMCP-capable descriptors
 *
 * Sends a GMCP event to every connected descriptor that has GMCP negotiated.
 * Colour codes are stripped from plain_text before embedding in the JSON body.
 *
 * @param channel_id   Channel identifier string (e.g. "gossip")
 * @param sender_name  Name of the sending character
 * @param plain_text   Message text (may contain internal colour codes)
 * @param timestamp    Unix timestamp for the message
 */
void channel_gmcp_broadcast(const char *channel_id, const char *sender_name,
                            const char *plain_text, time_t timestamp)
{
    DESCRIPTOR_DATA *d;
    char stripped[MSL];
    char esc_channel[MSL];
    char esc_sender[MSL];
    char esc_text[MSL];
    char json_body[MSL + 128];
    const char *nc;

    if (!channel_id || !sender_name || !plain_text)
        return;

    nc = nocolour(plain_text);
    strncpy(stripped, nc ? nc : plain_text, sizeof(stripped) - 1);
    stripped[sizeof(stripped) - 1] = '\0';

    json_escape_str(channel_id,   esc_channel, sizeof(esc_channel));
    json_escape_str(sender_name,  esc_sender,  sizeof(esc_sender));
    json_escape_str(stripped,     esc_text,    sizeof(esc_text));

    snprintf(json_body, sizeof(json_body),
             "{\"channel\":\"%s\",\"sender\":\"%s\",\"text\":\"%s\",\"timestamp\":%ld}",
             esc_channel, esc_sender, esc_text, (long)timestamp);

    for (d = descriptor_list; d != NULL; d = d->next) {
        if (!d->character)
            continue;
        if (!d->pProtocol || !d->pProtocol->bGMCP)
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

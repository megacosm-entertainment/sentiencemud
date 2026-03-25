#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../merc.h"
#include "../protocol.h"
#include "../gmcp_sentience.h"
#include "../account/preferences.h"
#include "channel_gmcp.h"
#include "channel_registry.h"

extern char *nocolour(const char *string);

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
    const char *nc;

    if (!def || !sender || !plain_text)
        return;

    nc = nocolour(plain_text);
    strncpy(stripped, nc ? nc : plain_text, sizeof(stripped) - 1);
    stripped[sizeof(stripped) - 1] = '\0';

    for (d = descriptor_list; d != NULL; d = d->next) {
        if (!d->character)
            continue;
        if (!d->pProtocol || !d->pProtocol->bGMCP)
            continue;
        if (!d->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE])
            continue;
        if (!pref_gmcp_channels(d->character))
            continue;
        if (!pref_check_channel(d->character, def->id))
            continue;
        if (!channel_gmcp_recipient_in_scope(def, sender, d->character))
            continue;

        sentience_channel_message_input_t input = {
            .channel     = def->id,
            .sender      = sender->name,
            .text        = stripped,
            .timestamp   = (long)timestamp,
            .tell_target = NULL,
        };

        sentience_send_package(d, "Sentience.Channel.Message",
            sentience_build_channel_message(&input));
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
    const char *nc;

    if (!sender || !recipient || !channel_id || !plain_text)
        return;

    nc = nocolour(plain_text);
    strncpy(stripped, nc ? nc : plain_text, sizeof(stripped) - 1);
    stripped[sizeof(stripped) - 1] = '\0';

    for (d = descriptor_list; d != NULL; d = d->next) {
        if (!d->character)
            continue;
        if (d->character != sender && d->character != recipient)
            continue;
        if (!d->pProtocol || !d->pProtocol->bGMCP)
            continue;
        if (!d->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE])
            continue;
        if (!pref_gmcp_channels(d->character))
            continue;

        sentience_channel_message_input_t input = {
            .channel     = channel_id,
            .sender      = sender->name,
            .text        = stripped,
            .timestamp   = (long)timestamp,
            .tell_target = recipient->name,
        };

        sentience_send_package(d, "Sentience.Channel.Message",
            sentience_build_channel_message(&input));
    }
}

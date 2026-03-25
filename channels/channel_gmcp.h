#ifndef CHANNEL_GMCP_H
#define CHANNEL_GMCP_H

#include "../merc.h"
#include "channel_registry.h"
#include <time.h>

/**
 * channel_gmcp_broadcast - Fire Sentience.Channel.Message to GMCP-capable descriptors
 *
 * Sends a GMCP event to descriptors that have GMCP negotiated and are within
 * the channel's scope (room, area, global, etc.) relative to the sender.
 *
 * @param def          Channel definition (for scope filtering)
 * @param sender       Sending character (for scope context)
 * @param plain_text   Message text (may contain internal colour codes)
 * @param timestamp    Unix timestamp for the message
 */
void channel_gmcp_broadcast(const CHANNEL_DEF_DATA *def, CHAR_DATA *sender,
                            const char *plain_text, time_t timestamp);

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
                                time_t timestamp);

#endif /* CHANNEL_GMCP_H */

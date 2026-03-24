#ifndef CHANNEL_GMCP_H
#define CHANNEL_GMCP_H

#include "../merc.h"
#include <time.h>

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
